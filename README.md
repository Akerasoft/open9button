# Open9button: NES Classic controller with HOME button to Wiimote/NES Classic adapter firmware

This is a firmware for using a NES controller on a Wii/Wii U through a Wii remote. The NES Classic edition
is also supported.

## Project homepage

This is based on Extenmote
English: [Extenmote](http://www.raphnet.net/electronique/extenmote/index_en.php)
French: [Extenmote](http://www.raphnet.net/electronique/extenmote/index.php)

This derivative is a controller pcb board or an arcade board.

Schematics and additional information such as button mappings are available on the project homepage:

English: TODO

## Supported micro-controller(s)

Currently supported micro-controller(s):

* Atmega8
* Atmega168

Adding support for other micro-controllers should be easy, as long as the target has enough
IO pins and enough memory (flash and SRAM).

For N64 and Gamecube controller compatiblity, the micro-controller must be clocked at 12 MHz.

## Built with

* [avr-gcc](https://gcc.gnu.org/wiki/avr-gcc)
* [avr-libc](http://www.nongnu.org/avr-libc/)
* [gnu make](https://www.gnu.org/software/make/manual/make.html)

## License

This project is licensed under the terms of the GNU General Public License, version 3.

## Acknowledgments

* Thanks to the authors of the [wiibrew wiki](http://wiibrew.org/wiki/Wiimote/Extension_Controllers/Classic_Controller) the excellent Wiimote accessory protocol documentation.
* Thanks to [Raphaël Assenat] and his project (https://www.raphnet.net/electronique/extenmote/index_en.php) for the original work.
