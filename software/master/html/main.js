(function ($) {
  $.each(["show", "hide"], function (i, ev) {
    var el = $.fn[ev];
    $.fn[ev] = function () {
      this.trigger(ev);
      return el.apply(this, arguments);
    };
  });
})(jQuery);

$(document).ready(() => {
  let currentSlotId = undefined;

  $("body").on("click", "nav>button", (e) => {
    $("nav>button").removeClass("selected");
    $(e.target).addClass("selected");
  });

  $("body").on("click", "button.toggle", (e) => {
    $(e.target).toggleClass("on");
  });

  $("body").on("click", "button[view]", (e) => {
    const view = $(e.target).attr("view");
    $(view).parent().children().hide();
    $(view).show();
    currentSlotId = undefined;
    if (view.startsWith("#slot-")) {
      currentSlotId = Number($(view).attr("slot"));
      term.reset();
      $("#terminal").show();
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
    cursorBlink: "block",
  });
  term.open(document.getElementById("terminal"));

  term.onKey((event) => {
    socket.emit("terminal", "0", event.key);
  });

  /* connect socket.io to server */
  const socket = io.connect("http://rack:8000");

  socket.on("slots", (data) => {
    const template = $("#template-slot").html();
    for (let slotId = 0; slotId < data.length; slotId++) {
      slots.push(data[slotId]);
      $("main").prepend($(template).attr("id", `slot-${slotId}`));
      $(`#slot-${slotId} .label`).val(data[slotId].label);
      $(`#slot-${slotId}`).attr("slot", slotId);
      if (slotId === 0) {
        $(`#slot-${slotId} .master-hidden`).hide();
      }
    }
  });
  socket.on("terminal", (slotId, data) => {
    slots[slotId].lastlog += data;
    if (slotId === currentSlotId) {
      term.write(data);
    }
  });
  socket.on("U", (slotId, value) => {
    $(`#slot-${slotId} .measurement .U`).text((value || 0).toFixed(1));
  });
  socket.on("I", (slotId, value) => {
    $(`#slot-${slotId} .measurement .I`).text((value || 0).toFixed(2));
  });
  socket.on("P", (slotId, value) => {
    $(`#slot-${slotId} .measurement .P`).text((value || 0).toFixed(0));
  });
});
