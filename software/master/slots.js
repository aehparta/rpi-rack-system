const {spi} = require('./io');
const config = require('./config');

const slots = [];

const slotRead = (slotId, callback) => {
  const slot = slots[slotId];
  const sendBuffer = Buffer.concat(
    [Buffer.from([0x01, 0x02]), slot.inputQueue.pop() || Buffer.from([])],
    10
  );
  spi.transfer(slotId, sendBuffer, data => {
    slot.ok = data[1] & 0x80 ? true : false;
    slot.hasCard = data[1] & 0x40 ? true : false;
    slot.U += 15.6;
    slot.I += data[2] >= 2 ? (data[2] - 2) * 8.2 : 0;
    slot.count++;

    let str = '';
    const noInvalids = data.filter(
      (ch, index) => ch > 7 && index != 1 && index != 2
    );
    if (noInvalids.length > 0) {
      str = Buffer.from(noInvalids).toString();
      /* append to log */
      const lastlog = slots[slotId].lastlog;
      str.split(/(?=\n)/g).forEach(part => {
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

const check = callback => {
  const slotCheck = slot => {
    spi.transfer(slot.id, Buffer.from([0x01, 0x02]), data => {
      slots[slot.id].ok = data[1] & 0x80 ? true : false;
      slots[slot.id].hasCard = data[1] & 0x40 ? true : false;
      slots[slot.id + 1] !== undefined
        ? slotCheck(slots[slot.id + 1])
        : callback();
    });
  };
  slotCheck(slots[0]);
};

const debugPrint = () => {
  slots.forEach(slot => {
    if (slot.ok) {
      console.log(
        ` - ${slot.label} (#${slot.id}): OK, ${
          slot.hasCard ? 'has card connected' : 'no card'
        }`
      );
    } else {
      console.log(` - slot #${slot.id}: ERROR`);
    }
  });
};

module.exports = {
  check,
  debugPrint,
  ok: () => slots.filter(slot => slot.ok),
  hasCard: () => slots.filter(slot => slot.hasCard),
  error: () => slots.filter(slot => !slot.ok),
  filter: cb => slots.filter(cb),
  forEach: cb => slots.forEach(cb),
  map: cb => slots.map(cb),
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
    I: 0,
    U: 0,
    P: 0,
    count: 0,

    read: callback => {
      slotRead(slotId, callback);
    },
  });
}

slots.forEach(slot => {
  module.exports[slot.id] = slot;
});
