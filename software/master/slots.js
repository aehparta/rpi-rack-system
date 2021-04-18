const { spi } = require('./io');
const config = require('./config');

const slots = [];

const slotTransfer = (slotId, callback) => {
  const slot = slots[slotId];
  const sendFirstCh = slot.inputQueue.pop() || 0;
  const sendBuffer = Buffer.concat([
    Buffer.from([sendFirstCh]),
    Buffer.alloc(15),
  ]);
  spi.transfer(slot.id, sendBuffer, (data) => {
    slot.bytesTransferred += data.length;
    // sendFirstCh != 0x11 && console.log(sendBuffer, data);

    const noInvalids = [];

    data.forEach((ch) => {
      if (ch === 0) {
        /* just ignore fully if zero */
      } else if (slot.lastByte === 0x11) {
        slot.statusUpdates++;
        slot.status = ch;
        slot.ok = ch & 0x80 ? true : false;
        slot.hasCard = slot.ok && ch & 0x40 ? true : false;
        slot.powered = slot.ok && ch & 0x10 ? true : false;
        slot.poweredDefault = slot.ok && ch & 0x01 ? true : false;
        slot.lastByte = 0;
      } else if (slot.lastByte === 0x12) {
        if (slot.powered && ch >= 2 && ch < 255) {
          slot.I += (ch - 2) * 8.2;
          slot.Icount++;
        }
        slot.U += 15.6;
        slot.Ucount++;
        slot.lastByte = 0;
      } else {
        if (ch !== 0x11 && ch !== 0x12 && ch > 7) {
          noInvalids.push(ch);
        }
        slot.lastByte = ch;
      }
    });

    const str = Buffer.from(noInvalids).toString();
    if (str.length > 0) {
      /* append to log */
      const lastlog = slots[slotId].lastlog;
      str.split(/(?=\n)/g).forEach((part) => {
        if (lastlog[lastlog.length - 1].slice(-1) !== '\n') {
          lastlog[lastlog.length - 1] += part;
        } else {
          lastlog.push(part);
        }
      });
      /* shorten log to maximum lines */
      while (lastlog.length > 1000) {
        lastlog.shift();
      }
    }

    callback(str);
  });
};

const debugPrint = () => {
  slots.forEach((slot) => {
    if (slot.ok) {
      console.log(
        ` - ${slot.label} (#${slot.id}): OK, ${
          slot.hasCard ? 'has card connected' : 'no card'
        }`
      );
    } else {
      console.log(` - slot #${slot.id}: ERROR (status raw: ${slot.statusRaw})`);
    }
  });
};

module.exports = {
  debugPrint,
  ok: () => slots.filter((slot) => slot.ok),
  hasCard: () => slots.filter((slot) => slot.hasCard),
  error: () => slots.filter((slot) => !slot.ok),
  filter: (cb) => slots.filter(cb),
  forEach: (cb) => slots.forEach(cb),
  map: (cb) => slots.map(cb),
  [Symbol.iterator]: () => {
    let i = 0;
    return {
      next: () => ({
        done: i >= slots.length,
        value: slots[++i],
      }),
    };
  },
};

for (let slotId = 0; slotId < config.slots.length; slotId++) {
  slots.push({
    ...config.slots[slotId],
    id: slotId,
    ok: false,
    hasCard: false,

    inputQueue: [],
    lastlog: [''],
    U: 0,
    Ucount: 0,
    I: 0,
    Icount: 0,
    P: 0,

    lastByte: 0,
    statusUpdates: 0,
    bytesTransferred: 0,

    select: (callback) => {
      spi.select(slotId, callback);
    },
    transfer: (callback) => {
      slotTransfer(slotId, callback);
    },
  });
}

slots.forEach((slot) => {
  module.exports[slot.id] = slot;
});
