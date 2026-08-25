// Dump raw bytes and ASCII over an address range.
// @category EricGrahamJuggler

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;

public class DumpBytesRange extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 3) {
			println("usage: DumpBytesRange <start> <count> <output-file>");
			return;
		}
		Address start = currentProgram.getAddressFactory().getAddress(args[0]);
		int count = Integer.decode(args[1]);
		File outFile = new File(args[2]);
		File parent = outFile.getParentFile();
		if (parent != null) {
			parent.mkdirs();
		}

		PrintWriter out = new PrintWriter(outFile, "UTF-8");
		try {
			for (int offset = 0; offset < count; offset += 16) {
				Address addr = start.add(offset);
				StringBuilder hex = new StringBuilder();
				StringBuilder ascii = new StringBuilder();
				for (int i = 0; i < 16 && offset + i < count; i++) {
					byte b = currentProgram.getMemory().getByte(addr.add(i));
					hex.append(String.format("%02x ", b & 0xff));
					int c = b & 0xff;
					ascii.append(c >= 32 && c < 127 ? (char)c : '.');
				}
				out.printf("%s  %-48s  %s%n", addr, hex.toString(), ascii.toString());
			}
		}
		finally {
			out.close();
		}
		println("wrote " + outFile.getAbsolutePath());
	}
}
