/* low level IO related stuff */
const SPI = require('spi-device');
const GPIO = require('onoff').Gpio;

const cs = [
  new GPIO(18, 'out'),
  new GPIO(23, 'out'),
  new GPIO(24, 'out'),
  new GPIO(25, 'out'),
];
cs.forEach(pin => pin.writeSync(1));

const rst = new GPIO(7, 'out');
rst.writeSync(1);

const select = (value, callback) => {
  Promise.all([
    cs[0].write(value & 0x01 ? 1 : 0),
    cs[1].write(value & 0x02 ? 1 : 0),
    cs[2].write(value & 0x04 ? 1 : 0),
    cs[3].write(value & 0x08 ? 1 : 0),
  ]).then(callback);
};

/* SPI */
const spiDevice = SPI.openSync(0, 0);

const transfer = (slotId, data, callback) => {
  select(slotId, () => {
    const buffer = [
      {
        sendBuffer: data,
        receiveBuffer: Buffer.alloc(data.length),
        byteLength: data.length,
        speedHz: 5e5,
      },
    ];
    spiDevice.transfer(buffer, err => {
      if (err) {
        throw err;
      }
      callback(buffer[0].receiveBuffer);
    });
  });
};

/* module */
module.exports = {
  spi: {
    select,
    transfer,
  },
};
