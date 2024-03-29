/* initialize terminal */
const term = new Terminal({ cursorBlink: 'block' });

/* connect socket.io to server */
const socket = io();

/* initialize app */
const app = new Vue({
  el: '#app',
  created() {
    socket.on(
      'slots',
      (data) =>
        (this.slots = data.map((slot) => ({
          ...slot,
          select: () => this.showSlot(slot.id),
          powerToggle: () => this.slotPowerToggle(slot.id),
        })))
    );
    socket.on('status', (id, status) => {
      this.slots = this.slots.map((slot) =>
        slot.id == id
          ? {
              ...slot,
              ...status,
            }
          : slot
      );
      if (this.slot?.id === id) {
        this.slot = this.slots[this.slot.id];
      }
    });
    socket.on('terminal', (id, data) => {
      const slot = this.slots[id];
      slot.lastlog += data;
      if (id === this.slot?.id) {
        term.write(data);
      }
    });
    socket.on('fans-speed', (speed) => {
      this.fans.speed = Number(speed);
    });
    term.onKey((event) => {
      socket.emit('terminal', this.slot.id, event.key);
    });
    setInterval(() => {
      const U = this.slots[0].U || 0;
      const I = this.slots.reduce((I, slot) => I + (slot.I || 0), 0);
      this.overview = {
        U,
        I,
        P: U * I,
      };
    }, 1000);
  },
  data: {
    overview: {
      U: 0,
      I: 0,
      P: 0,
    },
    fans: {
      speed: 0x00,
      speeds: [
        { value: 0x00, label: 'Off' },
        { value: 0x04, label: 'Stealth' },
        { value: 0x08, label: 'Quiet' },
        { value: 0x0c, label: 'Loud' },
        { value: 0x10, label: 'Annoying' },
      ],
    },
    slots: [],
    slot: undefined,
    view: 'overview',
  },
  methods: {
    showSlot(id) {
      /* if this slot is already selected */
      if (id === this.slot?.id) {
        return;
      }
      /* update data */
      this.slots = this.slots.map((slot) => ({
        ...slot,
        selected: id === slot.id,
      }));
      this.slot = this.slots[id];
      this.view = 'slot';
      /* reset terminal after the view has rendered properly */
      this.$nextTick(() => {
        term.reset();
        term.resize(80, 30);
        term.write(this.slot.lastlog);
        setTimeout(() => {
          term.scrollToBottom();
          term.focus();
        }, 100);
      });
    },
    showOverview() {
      if (this.slot) {
        this.slot.selected = false;
      }
      this.slot = undefined;
      this.view = 'overview';
    },
    slotPowerToggle(id) {
      socket.emit('power', id);
    },
    fansSpeedChange() {
      socket.emit('fans-speed', this.fans.speed);
    },
  },
});

term.open(document.getElementById('terminal'));
