(function ($) {
  $.each(['show', 'hide'], function (i, ev) {
    var el = $.fn[ev];
    $.fn[ev] = function () {
      this.trigger(ev);
      return el.apply(this, arguments);
    };
  });
})(jQuery);

$(document).ready(() => {
  let currentSlotId = undefined;

  $('body').on('click', 'nav>button', (e) => {
    $('nav>button').removeClass('selected');
    $(e.target).addClass('selected');
  });

  $('body').on('click', 'button.toggle', (e) => {
    $(e.target).toggleClass('on');
  });

  $('body').on('click', 'button[view]', (e) => {
    const view = $(e.target).attr('view');
    $(view).parent().children().hide();
    $(view).show();
    currentSlotId = undefined;
    if (view.startsWith('#slot-')) {
      currentSlotId = Number($(view).attr('slot'));
      term.reset();
      $('#terminal').show();
      term.resize(80, 30);
      term.write(slots[currentSlotId].lastlog);
    }
  });

  /* slots array */
  const slots = [];

  /* initialize terminal */
  const term = new Terminal({
    cols: 40,
    rows: 30,
    cursorBlink: 'block',
  });
  term.open(document.getElementById('terminal'));

  term.onKey((event) => {
    socket.emit('terminal', '0', event.key);
  });

  /* connect socket.io to server */
  const socket = io();

  socket.on('slots', (data) => {
    const template = $('#template-slot').html();
    for (let slotId = 0; slotId < data.length; slotId++) {
      slots.push(data[slotId]);
      $('main').prepend($(template).attr('id', `slot-${slotId}`));
      $(`#slot-${slotId} .label`).val(data[slotId].label);
      $(`#slot-${slotId}`).attr('slot', slotId);
      if (slotId === 0) {
        $(`#slot-${slotId} .master-hidden`).hide();
      }
    }
  });
  socket.on('terminal', (slotId, data) => {
    slots[slotId].lastlog += data;
    if (slotId === currentSlotId) {
      term.write(data);
    }
  });
  socket.on('status', (slotId, status) => {
    slots[slotId] = {
      ...slots[slotId],
      ...status,
    };
    const slot = slots[slotId];
    $(`#slot-${slot.id} .measurement .U`).text((slot.U || 0).toFixed(2));
    $(`#slot-${slot.id} .measurement .I`).text((slot.I || 0).toFixed(3));
    $(`#slot-${slot.id} .measurement .P`).text((slot.P || 0).toFixed(1));
  });

  setInterval(() => {
    const U = slots[0].U || 0;
    const I = slots.reduce((I, slot) => I + (slot.I || 0), 0);
    const P = U * I;
    $(`#overview .measurement .U`).text(U.toFixed(2));
    $(`#overview .measurement .I`).text(I.toFixed(3));
    $(`#overview .measurement .P`).text(P.toFixed(1));
  }, 1000);
});
