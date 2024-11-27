if (typeof(arg) === "undefined") {
	// === NODE.JS SHIM ===

	arg = process.argv.slice(2); // remove the node args

	print = function(str) { console.log(str); };

	fs = require('fs');
	listdir = function(path) { return fs.readdirSync(path); };
	read = function(path) { return fs.readFileSync(path, "utf8"); };
	write = function(path, str) { fs.writeFileSync(path, str); };
}

var srcPath = arg[0];
var destPath = arg[1];

print("=== embed: " + srcPath + " -> " + destPath + " ===");

var srcPathSplit = srcPath.split("/");
var embedName = srcPathSplit[srcPathSplit.length - 1];
var embedHeaderFileName = embedName + ".h";
var embedDef = embedName.toUpperCase() + "_H";

var embedHeaderStr = "";
embedHeaderStr += "#ifndef " + embedDef + "\n#define " + embedDef + "\n\n";

var files = listdir(srcPath);
for (var i = 0; i < files.length; i++) {
	var fileName = files[i];
	var fileExt = fileName.split(".")[1];

	if (fileExt === "js" || fileExt === "bitsy" || fileExt === "bitsyfont") {
		print(fileName);

		embedHeaderStr += "char* " + (fileName.replace(".", "_")) + " =\n";

		var fileSrc = read(srcPath + "/" + fileName);
		var fileLines = fileSrc.split(/\r?\n/);

		for (var j = 0; j < fileLines.length; j++) {
			var lineStr = fileLines[j];
			lineStr = lineStr.replace(/\\/g, '\\\\');
			lineStr = lineStr.replace(/"/g, '\\"');
			embedHeaderStr += "\t\"" + lineStr + "\\n\"" + (j < fileLines.length - 1 ? "\n" : ";\n\n");
		}
	}
}

embedHeaderStr += "#endif";

write(destPath + "/" + embedHeaderFileName, embedHeaderStr);

print("=== done! ===");