// Export a compact reverse-engineering report for headless comparison.
// @category EricGrahamJuggler

import java.io.File;
import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileOptions;
import ghidra.app.decompiler.DecompileResults;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Data;
import ghidra.program.model.listing.Function;
import ghidra.program.model.listing.FunctionIterator;
import ghidra.program.model.mem.MemoryBlock;
import ghidra.program.model.symbol.Reference;
import ghidra.program.model.symbol.ReferenceIterator;
import ghidra.program.model.symbol.Symbol;
import ghidra.program.model.symbol.SymbolIterator;

public class ExportREReport extends GhidraScript {
	@Override
	protected void run() throws Exception {
		String[] args = getScriptArgs();
		if (args.length < 1) {
			println("usage: ExportREReport <output-file>");
			return;
		}

		File outFile = new File(args[0]);
		File parent = outFile.getParentFile();
		if (parent != null) {
			parent.mkdirs();
		}

		PrintWriter out = new PrintWriter(outFile, "UTF-8");
		try {
			exportReport(out);
		}
		finally {
			out.close();
		}
		println("wrote " + outFile.getAbsolutePath());
	}

	private void exportReport(PrintWriter out) throws Exception {
		out.println("# " + currentProgram.getName());
		out.println();
		out.println("Executable path: " + currentProgram.getExecutablePath());
		out.println("Language: " + currentProgram.getLanguageID());
		out.println("Compiler spec: " + currentProgram.getCompilerSpec().getCompilerSpecID());
		out.println("Image base: " + currentProgram.getImageBase());
		out.println();

		out.println("## Memory blocks");
		for (MemoryBlock block : currentProgram.getMemory().getBlocks()) {
			out.println(block.getName() + " " + block.getStart() + " " + block.getEnd() +
				" size=" + block.getSize() + " rxw=" +
				(block.isRead() ? "r" : "-") + (block.isExecute() ? "x" : "-") +
				(block.isWrite() ? "w" : "-"));
		}
		out.println();

		out.println("## External symbols");
		List<Symbol> externals = new ArrayList<Symbol>();
		SymbolIterator symbols = currentProgram.getSymbolTable().getAllSymbols(true);
		while (symbols.hasNext()) {
			Symbol sym = symbols.next();
			if (sym.isExternal()) {
				externals.add(sym);
			}
		}
		Collections.sort(externals, Comparator.comparing(Symbol::getName));
		for (Symbol sym : externals) {
			out.println(sym.getName() + " " + sym.getAddress());
		}
		out.println();

		out.println("## Strings");
		for (Data data : currentProgram.getListing().getDefinedData(true)) {
			Address addr = data.getAddress();
			Object value = data.getValue();
			if (!(value instanceof String)) {
				continue;
			}
			out.println(addr + " " + quote(value == null ? "" : value.toString()));
			ReferenceIterator refs = currentProgram.getReferenceManager().getReferencesTo(addr);
			while (refs.hasNext()) {
				Reference ref = refs.next();
				Function fn = currentProgram.getFunctionManager().getFunctionContaining(ref.getFromAddress());
				out.println("  ref " + ref.getFromAddress() + " " + (fn == null ? "<none>" : fn.getName()));
			}
		}
		out.println();

		out.println("## Functions");
		FunctionIterator functions = currentProgram.getFunctionManager().getFunctions(true);
		while (functions.hasNext()) {
			Function fn = functions.next();
			out.println(fn.getEntryPoint() + " " + fn.getName() + " " + fn.getBody().getNumAddresses());
		}
		out.println();

		out.println("## Decompiled functions");
		DecompInterface decompiler = new DecompInterface();
		DecompileOptions options = new DecompileOptions();
		options.grabFromProgram(currentProgram);
		decompiler.setOptions(options);
		decompiler.toggleCCode(true);
		decompiler.toggleSyntaxTree(false);
		decompiler.setSimplificationStyle("decompile");
		if (!decompiler.openProgram(currentProgram)) {
			out.println("Decompiler open failed: " + decompiler.getLastMessage());
			return;
		}
		try {
			functions = currentProgram.getFunctionManager().getFunctions(true);
			while (functions.hasNext()) {
				Function fn = functions.next();
				if (fn.isExternal()) {
					continue;
				}
				monitor.checkCancelled();
				DecompileResults results = decompiler.decompileFunction(fn, 30, monitor);
				out.println();
				out.println("### " + fn.getName() + " " + fn.getEntryPoint());
				if (results.decompileCompleted() && results.getDecompiledFunction() != null) {
					out.println(results.getDecompiledFunction().getC());
				}
				else {
					out.println("/* decompile failed: " + results.getErrorMessage() + " */");
				}
			}
		}
		finally {
			decompiler.dispose();
		}
	}

	private String quote(String s) {
		return "\"" + s.replace("\\", "\\\\").replace("\n", "\\n").replace("\r", "\\r").replace("\"", "\\\"") + "\"";
	}
}
