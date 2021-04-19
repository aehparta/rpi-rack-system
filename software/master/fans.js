const { spi } = require('./io');

const state = {
  U: 0,
  status: 0x00,
  speed: 0x07,
  lastByte: undefined,
};

const transfer = (callback) => {
  const sendBuffer = Buffer.from([
    0x11,
    0x80 | state.status,
    0x12,
    0x80 | state.speed,
  ]);
  spi.transfer(14, sendBuffer, (data) => {
    data.forEach((ch) => {
      if (ch === 0) {
        /* just ignore fully if zero */
      } else if (state.lastByte === 0x11) {
        state.U = ch * 0.1184;
        state.lastByte = 0;
      } else {
        state.lastByte = ch;
      }
    });
    callback();
  });
};

module.exports = {
  get U() {
    return state.U;
  },
  get speed() {
    return state.speed;
  },
  set speed(value) {
    state.speed = value;
  },
  transfer: (callback) => transfer(callback),
};
