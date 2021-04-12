const {spawn} = require('child_process');
const fs = require('fs');
const slots = require('./slots');

const binaryFileKeeperSlot = '../keepers/keeper-slot-AVR.elf';
const binaryFileKeeperFans = '../keepers/keeper-fans-AVR.elf';
const binaryFileBattery = '../keepers/battery-AVR.elf';

console.log('Checking binaries:');
try {
  fs.statSync(binaryFileKeeperSlot);
  fs.statSync(binaryFileKeeperFans);
  // fs.statSync(binaryFileBattery);
} catch (e) {
  console.log(e);
  process.exit(1);
}
console.log(' - binaries OK');

console.log('Checking slots:');
slots.check(() => {
  slots.debugPrint();
});

// console.log("Starting upgrade:");
// console.log("");
// const upgrade = (slotId) => {
//   const slot = slots[slotId];
//   if (slot === undefined) {
//     return;
//   }
//   if (!slot.ok) {
//     upgrade(slotId + 1);
//     return;
//   }

//   slot.select();
//   console.log(`### slot #${slot.id}: upgrading ###`);

//   const args = [
//     "avrdude",
//     "-q",
//     "-p",
//     "atmega328p",
//     "-c",
//     "linuxspi",
//     "-P",
//     "/dev/spidev0.0",
//     "-U",
//     `flash:w:${binaryFileKeeperSlot}`,
//   ];
//   const cmd = spawn("sudo", args);

//   cmd.stdout.on("data", (data) => {
//     process.stdout.write(data);
//   });
//   cmd.stderr.on("data", (data) => {
//     process.stderr.write(data);
//   });

//   cmd.on("close", (code) => {
//     if (code !== 0) {
//       process.exit(1);
//     }
//     upgrade(slotId + 1);
//   });
// };
// upgrade(0);
