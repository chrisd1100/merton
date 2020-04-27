import xml.etree.ElementTree
import struct

def xml_to_bin(name):
	data = []
	num_rows = 0

	for game in xml.etree.ElementTree.parse(name).getroot():
		rom = game.find('rom')
		prg = game.find('prgrom')
		prgram = game.find('prgram')
		prgnvram = game.find('prgnvram')
		chrrom = game.find('chrrom')
		chrram = game.find('chrram')
		chrnvram = game.find('chrnvram')
		pcb = game.find('pcb')

		prgrom_size = int(int(prg.get('size')) / 16384)
		chrrom_size = int(int(chrrom.get('size')) / 8192) if chrrom is not None else 0

		if prgrom_size < 256 and chrrom_size < 256:
			data += struct.pack('I', int(rom.get('crc32'), 16))
			data += struct.pack('B', prgrom_size)
			data += struct.pack('H', int(int(prgram.get('size')) / 8) if prgram is not None else 0)
			data += struct.pack('H', int(int(prgnvram.get('size')) / 8) if prgnvram is not None else 0)
			data += struct.pack('B', chrrom_size)
			data += struct.pack('H', int(int(chrram.get('size')) / 8) if chrram is not None else 0)
			data += struct.pack('H', int(int(chrnvram.get('size')) / 8) if chrnvram is not None else 0)
			data += struct.pack('H', int(pcb.get('mapper')))

			mirroring = pcb.get('mirroring')
			mirroring_bits = 0x10 if mirroring == 'V' else 0x20 if mirroring == '4' else 0x00
			data += struct.pack('B', int(pcb.get('submapper')) | mirroring_bits | (int(pcb.get('battery')) << 7))

			num_rows += 1

	return data, num_rows

data, num_rows = xml_to_bin('nes20db.xml')
row_len = int(len(data) / num_rows)

f = open('nes20db.h', 'w')
f.write('#pragma once\n\n')
f.write('static const unsigned int NES_DB_ROWS = %u;\n' % num_rows)
f.write('static const unsigned int NES_DB_ROW_SIZE = %u;\n' % row_len)
f.write('static const unsigned char %s[] = {%s};\n' % ('NES_DB', ', '.join('{:d}'.format(x) for x in data)))
