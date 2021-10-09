var fs = require('fs');

var args = process.argv.slice(2); // remove the node args
var srcPath = args[0];
var destPath = args[1];

console.log("=== embed: " + srcPath + " -> " + destPath + " ===");

var srcPathSplit = srcPath.split("/");
var embedName = srcPathSplit[srcPathSplit.length - 1];
var embedHeaderFileName = embedName + ".h";
var embedDef = embedName.toUpperCase() + "_H";

var embedHeaderStr = "";
embedHeaderStr += "#ifndef " + embedDef + "\n#define " + embedDef + "\n\n";

var files = fs.readdirSync(srcPath);
for (var i = 0; i < files.length; i++) {
	var fileName = files[i];
	var fileExt = fileName.split(".")[1];

	if (fileExt === "js" || fileExt === "bitsy" || fileExt === "bitsyfont") {
		console.log(fileName);
		// console.log(fs.readFileSync(srcPath + "/" + fileName, "utf8"));

		embedHeaderStr += "char* " + (fileName.replace(".", "_")) + " =\n";

		var fileSrc = fs.readFileSync(srcPath + "/" + fileName, "utf8");
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

fs.writeFileSync(destPath + "/" + embedHeaderFileName, embedHeaderStr);

console.log("=== done! ===");