/* eslint no-param-reassign: 0 */

/**
 * This is a default CosmoScout API. Once initialized, you can access its methods via
 * CosmoScout.gui.<method name>.
 */
class GuiApi extends IApi {
  /**
   * @inheritDoc
   */
  name = 'gui';

  /**
   * Cache loaded templates
   *
   * @see {loadTemplateContent}
   * @type {Map<string, DocumentFragment>}
   * @private
   */
  _templates = new Map();

  /**
   * Registered html parts
   *
   * @see {registerHtml}
   * @type {Map<string, DocumentFragment>}
   * @private
   */
  _html = new Map();

  /**
   * Initialize third party drop downs,
   * add input event listener,
   * initialize tooltips
   */
  initInputs() {
    this.initDropDowns();
    this.initChecklabelInputs();
    this.initRadiolabelInputs();
    this.initTooltips();
  }

  /**
   * Initializes all .simple-value-dropdown with bootstrap's selectpicker.
   *
   * @see {initInputs}
   */
  initDropDowns() {
    const dropdowns = $('.simple-value-dropdown');
    dropdowns.selectpicker();

    const eventListener = (event) => {
      if (event.target !== null) {
        let callback = CosmoScout.callbacks.find(event.target.dataset.callback)

        if (callback !== undefined) {
          callback(event.target.value);
        }
      }
    };

    document.querySelectorAll('.simple-value-dropdown').forEach((dropdown) => {
      if (typeof dropdown.dataset.initialized === 'undefined') {
        dropdown.addEventListener('change', eventListener);
      }
    });
  }

  /**
   * Adds a 'change' event listener which calls callNative with id and checkstate.
   * This will only add a listener once.
   *
   * @see {callNative}
   * @see {initInputs}
   */
  initChecklabelInputs() {
    document.querySelectorAll('.checklabel input').forEach((input) => {
      if (typeof input.dataset.initialized === 'undefined') {
        input.addEventListener('change', (event) => {
          if (event.target !== null) {
            let callback = CosmoScout.callbacks.find(event.target.dataset.callback)

            if (callback !== undefined) {
              callback(event.target.checked);
            }
          }
        });

        input.dataset.initialized = 'true';
      }
    });
  }

  /**
   * Adds a change event listener which calls callNative with the target id.
   *
   * @see {callNative}
   * @see {initInputs}
   */
  initRadiolabelInputs() {
    document.querySelectorAll('.radiolabel input').forEach((input) => {
      if (typeof input.dataset.initialized === 'undefined') {
        input.addEventListener('change', (event) => {
          if (event.target !== null) {
            let callback = CosmoScout.callbacks.find(event.target.dataset.callback)

            if (callback !== undefined) {
              callback(event.target.checked);
            }
          }
        });

        input.dataset.initialized = 'true';
      }
    });
  }

  /**
   * Initializes [data-toggle="tooltip"] elements.
   *
   * @see {initInputs}
   */
  initTooltips() {
    const config = {delay: 500, placement: 'auto', html: false};

    /* Bootstrap Tooltips require jQuery for now */
    $('[data-toggle="tooltip"]').tooltip(config);
    config.placement = 'bottom';
    $('[data-toggle="tooltip-bottom"]').tooltip(config);
  }

  /**
   * Appends a link stylesheet to the head.
   *
   * @param url {string}
   */
  registerCss(url) {
    const link = document.createElement('link');
    link.setAttribute('rel', 'stylesheet');
    link.setAttribute('href', url);

    document.head.appendChild(link);
  }

  /**
   * Removes a stylesheet by url.
   *
   * @param url {string}
   */
  unregisterCss(url) {
    document.querySelectorAll('link').forEach((element) => {
      if (typeof element.href !== 'undefined' && element.href === url) {
        document.head.removeChild(element);
      }
    });
  }

  /**
   * Append HTML to the body (default) or element with id containerId.
   *
   * @param id {string} Id for de-registering
   * @param content {string} Html content
   * @param containerId {string} ['body'] Container ID to append the HTML to. Defaults to body
   * element if omitted
   */
  registerHtml(id, content, containerId = 'body') {
    let container = document.body;
    if (containerId !== 'body') {
      container = document.getElementById(containerId);
    }

    if (container === null) {
      console.warn(`Cannot register #${id} into container #${containerId}!`);
      return;
    }

    const item = document.createElement('template');

    item.innerHTML = content;

    this._html.set(id, item.content);

    container.appendChild(item.content);
  }

  /**
   * Remove registered html from the body or container with id containerId.
   *
   * @see {registerHtml}
   * @param id {string}
   * @param containerId {string}
   */
  unregisterHtml(id, containerId = 'body') {
    let container = document.body;
    if (containerId !== 'body') {
      container = document.getElementById(containerId);
    }

    if (container === null) {
      console.warn(`Container #${containerId} does not exist!`);
      return;
    }

    if (!this._html.has(id)) {
      console.warn(`No Html with #${id} registered!`);
      return;
    }

    container.removeChild(this._html.get(id));
    this._html.delete(id);
  }

  /**
   * Tries to load the template content of 'id-template'.
   * Returns false if no template was found, HTMLElement otherwise.
   *
   * @param templateId {string} Template element id without '-template' suffix
   * @return {boolean|HTMLElement}
   */
  loadTemplateContent(templateId) {
    const id = `${templateId}-template`;

    if (this._templates.has(id)) {
      return this._templates.get(id).cloneNode(true).firstElementChild;
    }

    const template = document.getElementById(id);

    if (template === null) {
      console.warn(`Template '#${id}' not found!`);
      return false;
    }

    const {content} = template;
    this._templates.set(id, content);

    return content.cloneNode(true).firstElementChild;
  }

  /**
   * Clear the content of an element if it exists.
   *
   * @param element {string|HTMLElement} Element or ID
   * @return {void}
   */
  clearHtml(element) {
    if (typeof element === 'string') {
      // eslint-disable-next-line no-param-reassign
      element = document.getElementById(element);
    }

    if (element !== null && element instanceof HTMLElement) {
      while (element.firstChild !== null) {
        element.removeChild(element.firstChild);
      }
    } else {
      console.warn('Element could not be cleared.');
    }
  }

  /**
   * Toggle the class hidden on the given element.
   *
   * @param element {string|HTMLElement} Element or selector
   * @param element {string|HTMLElement} Element or ID
   * @return {void}
   */
  hideElement(element, hide) {
    if (typeof element === 'string') {
      // eslint-disable-next-line no-param-reassign
      element = document.querySelector(element);
    }

    if (element !== null && element instanceof HTMLElement) {
      if (hide) {
        element.classList.add('hidden');
      } else {
        element.classList.remove('hidden');
      }
    } else {
      console.warn('Element could not be shown / hidden!');
    }
  }

  /**
   * Initialize a noUiSlider.
   *
   * @param callbackName {string} tha data-callback attribute of the slider element
   * @param min {number} Min value
   * @param max {number} Max value
   * @param step {number} Step size
   * @param start {number[]} Handle count and position
   */
  initSlider(callbackName, min, max, step, start) {
    const slider = document.querySelector(`[data-callback="${callbackName}"]`);

    if (typeof noUiSlider === 'undefined') {
      console.warn('\'noUiSlider\' is not defined!');
      return;
    }

    noUiSlider.create(slider, {
      start,
      connect: (start.length === 1 ? 'lower' : true),
      step,
      range: {min, max},
      format: {
        to(value) {
          return CosmoScout.utils.beautifyNumber(value);
        },
        from(value) {
          return Number(parseFloat(value));
        },
      },
    });

    slider.noUiSlider.on('slide', (values, handle, unencoded) => {
      let callback = CosmoScout.callbacks.find(callbackName);
      if (callback !== undefined) {
        if (Array.isArray(unencoded)) {
          callback(unencoded[handle], handle);
        } else {
          callback(unencoded, 0);
        }
      }
    });
  }

  /**
   * Sets a noUiSlider value.
   *
   * @param callbackName {string} tha data-callback attribute of the slider element
   * @param value {number} Value
   */
  setSliderValue(callbackName, emitCallbacks, ...value) {
    const slider = document.querySelector(`[data-callback="${callbackName}"]`);

    if (slider !== null && typeof slider.noUiSlider !== 'undefined') {
      if (!slider.matches(":active")) {
        if (value.length === 1) {
          slider.noUiSlider.set(value[0], emitCallbacks);
        } else {
          slider.noUiSlider.set(value, emitCallbacks);
        }
      }
    } else {
      console.warn(`Slider '${callbackName} 'not found or 'noUiSlider' not active.`);
    }
  }

  /**
   * Clears the content of a selecticker dropdown.
   *
   * @param callbackName {string} tha data-callback attribute of the dropdown element
   */
  clearDropdown(callbackName) {
    const dropdown = document.querySelector(`[data-callback="${callbackName}"]`);
    CosmoScout.gui.clearHtml(dropdown);

    $(dropdown).selectpicker('render');
  }

  /**
   * Adds an option to a dropdown.
   *
   * @param callbackName {string} tha data-callback attribute of the dropdown element
   * @param value {string|number} Option value
   * @param text {string} Option text
   * @param selected {boolean|string} Selected flag
   */
  addDropdownValue(callbackName, value, text, selected = false) {
    const dropdown = document.querySelector(`[data-callback="${callbackName}"]`);
    const option   = document.createElement('option');

    option.value       = value;
    option.selected    = selected ? true : false;
    option.textContent = text;

    if (dropdown !== null) {
      dropdown.appendChild(option);

      $(dropdown).selectpicker('refresh');
    } else {
      console.warn(`Dropdown '${callbackName} 'not found`);
    }
  }

  /**
   * Sets the current value of a selectpicker.
   *
   * @param callbackName {string} tha data-callback attribute of the dropdown element
   * @param value {string|number}
   */
  setDropdownValue(callbackName, value, emitCallbacks) {
    const dropdown = document.querySelector(`[data-callback="${callbackName}"]`);
    $(dropdown).selectpicker('val', value);

    if (emitCallbacks) {
      this._emitChangeEvent(dropdown);
    }
  }

  /**
   * Sets a radio button to checked.
   *
   * @see {setCheckboxValue}
   * @param callbackName {string} tha data-callback attribute of the radio button element
   */
  setRadioChecked(callbackName, emitCallbacks) {
    this.setCheckboxValue(callbackName, true, emitCallbacks);
  }

  /**
   * Sets a checkboxs checked state to true/false.
   *
   * @param callbackName {string} tha data-callback attribute of the radio button element
   * @param value {boolean} True = checked / False = unchecked
   */
  setCheckboxValue(callbackName, value, emitCallbacks) {
    const element = document.querySelector(`[data-callback="${callbackName}"]`);

    if (element !== null) {
      element.checked = value;

      if (emitCallbacks) {
        this._emitChangeEvent(element);
      }
    }
  }

  /**
   * Sets the value of a text input.
   * Only selects .text-input s which descend .item-ID
   *
   * @param callbackName {string} tha data-callback attribute of the text input element
   * @param value {string}
   */
  setTextboxValue(id, value) {
    const element = document.querySelector(`.item-${id} .text-input`);

    if (element !== null) {
      element.value = value;
    }
  }

  /**
   * Triggers an artificial change event on a given HTML element.
   *
   * @param element {HTMLElement} The element to fire the event on
   * @private
   */
  _emitChangeEvent(element) {
    let evt = document.createEvent("HTMLEvents");
    evt.initEvent("change", false, true);
    element.dispatchEvent(evt);
  }
}
