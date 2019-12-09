#version 430

#extension GL_ARB_compute_variable_group_size: enable

// TODO make configurable
const uint MIN_WAVELENGTH = 390u;
const uint MAX_WAVELENGTH = 749u;
const uint NUM_WAVELENGTHS = MAX_WAVELENGTH - MIN_WAVELENGTH + 1;

// Size: 24 bytes -> ~40,000,000 photons per available gigabyte of ram
struct Photon {
    dvec3 position;// m
    dvec3 direction;// normalized
    double intensity;// 0..1 should start at 1
    uint wavelength;// nm
    uint _padding;
};

// maybe generate in GPU with rng?
layout(std430, binding = 0) buffer Photons {
    Photon photons[];
};

layout(std430, binding = 1) buffer RefractiveIndices {
    double[][NUM_WAVELENGTHS] refractiveIndicesAtAltitudes;// DX steps
};

layout(std430, binding = 2) buffer Densities {
    double[] densitiesAtAltitudes;
};

struct Planet {
    double radius;// m
    double atmosphericHeight;// m
    double seaLevelMolecularNumberDensity;// cm^−3
};

uniform Planet planet;

// TODO make configurable
const double DL = 1000.0LF;// m
const double DX = 10.0LF;// m

layout (local_size_variable) in;

double densityAtAltitude(double altitude) {
    return densitiesAtAltitudes[uint(altitude)];
}

double refractiveIndexAtSeaLevel(uint wavelength) {
    return refractiveIndicesAtAltitudes[0][wavelength - MIN_WAVELENGTH];
}

double refractiveIndexAtAltitude(double altitude, uint wavelength) {
    return refractiveIndicesAtAltitudes[uint(altitude)][wavelength - MIN_WAVELENGTH];
}

double partialRefractiveIndex(double altitude, double altitudeDelta, uint wavelength) {
    double refrIndexPlusDelta = refractiveIndexAtAltitude(altitudeDelta, wavelength);
    double refrIndex = refractiveIndexAtAltitude(altitude, wavelength);

    return (refrIndexPlusDelta - refrIndex) / DX;
}

/// Moves the photon to its next location.
void traceRay(inout Photon photon) {
    double altitude = length(photon.position) - planet.radius;
    double altDx = length(photon.position + vec3(DX, 0.0LF, 0.0LF)) - planet.radius;
    double altDy = length(photon.position + vec3(0.0LF, DX, 0.0LF)) - planet.radius;
    double altDz = length(photon.position + vec3(0.0LF, 0.0LF, DX)) - planet.radius;
    double altD1Approx = length(photon.position + DL * photon.direction) - planet.radius;

    if (altitude < planet.atmosphericHeight
        && altDx < planet.atmosphericHeight
        && altDy < planet.atmosphericHeight
        && altDz < planet.atmosphericHeight
        && altD1Approx < planet.atmosphericHeight) {

        double ni = refractiveIndexAtAltitude(altitude, photon.wavelength);
        double pnx = partialRefractiveIndex(altitude, altDx, photon.wavelength);
        double pny = partialRefractiveIndex(altitude, altDy, photon.wavelength);
        double pnz = partialRefractiveIndex(altitude, altDz, photon.wavelength);
        dvec3 dn = dvec3(pnx, pny, pnz);

        double ni1 = refractiveIndexAtAltitude(altD1Approx, photon.wavelength);
        dvec3 direction = ((ni * photon.direction) + (dn * DL)) / ni1;
        photon.direction = normalize(direction);
    }

    photon.position += (DL * photon.direction);
}

double molecularNumberDensityAtAltitude(double altitude) {
    double seaLevelDensity = densityAtAltitude(0.0LF);
    return planet.seaLevelMolecularNumberDensity * (densityAtAltitude(altitude) / seaLevelDensity);
}

/// I am to lazy to do the squaring every time my self.
double square(double value) {
    return value * value;
}

double rayleighScatteringCrossSection(uint wavelength) {
    // TODO Normally wavelength should be converted with a factor of 1.0e-7, but for no particular reason 2.1e-8 works best.
    //      Let's not talk about this :/
    double wavelengthInCM = double(wavelength) * 1.0e-7LF;
    double wavelengthInCM4 = square(square(wavelengthInCM));

    double refractiveIndex = double(refractiveIndexAtSeaLevel(wavelength));
    double refractiveIndex2 = refractiveIndex * refractiveIndex;

    double molecularNumberDensity = molecularNumberDensityAtAltitude(0.0);
    double molecularNumberDensity2 = square(molecularNumberDensity);

    const double kingCorrectionFactor = 1.05LF;
    const double PI_F = 3.14159265358979323846LF;
    const double PI_F_3 = PI_F * PI_F * PI_F;

    double dividend = 24.0LF * PI_F_3 * square(refractiveIndex2 - 1.0LF);
    double divisor = wavelengthInCM4 * molecularNumberDensity2 * square(refractiveIndex2 + 2.0LF);
    return (dividend / divisor) * kingCorrectionFactor;
}

// TODO maybe precompute in a 2D map?
double rayleighVolumeScatteringCoefficient(double altitude, uint wavelength) {
    double sigma = rayleighScatteringCrossSection(wavelength);
    double mnd = molecularNumberDensityAtAltitude(altitude);
    return mnd * sigma;
}

/// [2, 2] Padé approximant for exp(x).
/// Since there is no exp function for doubles, this is a good and fast approximation for small x.
/// This is enough for this particular application.
double approxE(double x) {
    return (square(x + 3.0LF) + 3.0LF) / (square(x - 3.0LF) + 3.0LF);
}

/// Applies rayleigh scattering to the photon for this step.
void attenuateLight(inout Photon photon, dvec3 oldPosition) {
    double altitude = length(oldPosition) - planet.radius;

    double beta = rayleighVolumeScatteringCoefficient(altitude, photon.wavelength);

    // TODO don't know what to do with this for now... maybe make it configurable per planet?
    /// This value simulates particles in the upper atmosphere. On earth a value of 1.0e-6 corresponds to an L4 eclipse
    /// and 1.0e-4 produces an L0 eclipse.
    double alpha = 15000.0LF < altitude && altitude < 20000.0LF ? 1.0e-5LF : 0.0LF;

    photon.intensity = photon.intensity * approxE(-(alpha + beta) * DL);
}

/// Does a single step of the ray tracing. It moves the photon to the next location and applies
/// rayleigh scattering to it.
void tracePhoton(inout Photon photon) {
    dvec3 oldPosition = dvec3(photon.position);

    traceRay(photon);
    attenuateLight(photon, oldPosition);
}

uniform uint pass;
uniform uint passSize;

void main() {
    uint gid = gl_GlobalInvocationID.x;
    uint passId = (pass * passSize) + gid;
    if (passId >= photons.length()) return;

    Photon photon = photons[passId];

    bool enteredAtmosphere = false;
    bool exitedAtmosphere = false;

    double atmosphereRadius = planet.radius + planet.atmosphericHeight;

    double distFromCenter = length(photon.position);
    while (!exitedAtmosphere && distFromCenter > planet.radius) {
        tracePhoton(photon);
        distFromCenter = length(photon.position);

        if (!enteredAtmosphere && distFromCenter < atmosphereRadius) {
            enteredAtmosphere = true;
        }

        if (enteredAtmosphere && distFromCenter > atmosphereRadius) {
            exitedAtmosphere = true;
        }
    }

    if (!exitedAtmosphere || distFromCenter <= planet.radius) {
        photon.intensity = -1.0LF;
    }

    photons[passId] = photon;
}