// Dump instructions and raw bytes over an address range.
// @category EricGrahamJuggler

import java.io.File;
import java.io.PrintWriter;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Instruction;

public class DumpAsmRange extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 3) {
			println("usage: DumpAsmRange <start> <end> <output-file>");
			return;
		}
		Address start = currentProgram.getAddressFactory().getAddress(args[0]);
		Address end = currentProgram.getAddressFactory().getAddress(args[1]);
		File outFile = new File(args[2]);
		File parent = outFile.getParentFile();
		if (parent != null) {
			parent.mkdirs();
		}

		PrintWriter out = new PrintWriter(outFile, "UTF-8");
		try {
			Address addr = start;
			while (addr.compareTo(end) <= 0) {
				monitor.checkCancelled();
				Instruction instr = currentProgram.getListing().getInstructionAt(addr);
				if (instr == null) {
					disassemble(addr);
					instr = currentProgram.getListing().getInstructionAt(addr);
				}
				if (instr != null) {
					out.printf("%s  %-24s ; %s%n", addr, instr.toString(), bytesAt(addr, instr.getLength()));
					addr = addr.add(instr.getLength());
				}
				else {
					out.printf("%s  %-24s ; %s%n", addr, "dc.w", bytesAt(addr, 2));
					addr = addr.add(2);
				}
			}
		}
		finally {
			out.close();
		}
		println("wrote " + outFile.getAbsolutePath());
	}

	private String bytesAt(Address addr, int count) {
		StringBuilder sb = new StringBuilder();
		for (int i = 0; i < count; i++) {
			if (i != 0) {
				sb.append(' ');
			}
			try {
				sb.append(String.format("%02x", currentProgram.getMemory().getByte(addr.add(i)) & 0xff));
			}
			catch (Exception e) {
				sb.append("??");
			}
		}
		return sb.toString();
	}
}
