#target illustrator

/* Illustrator: File > Scripts > Other Script. The SVG is the editable geometry master. */
(function () {
    var folder = new File($.fileName).parent;
    var project = folder.parent.parent;
    var evidence = new Folder(project.fsName + "/build/app-icon");
    evidence.create();
    var source = new File(folder.fsName + "/Congregation.svg");
    var ai = new File(folder.fsName + "/Congregation.ai");
    var png = new File(folder.fsName + "/Congregation.png");
    var report = new File(evidence.fsName + "/illustrator-export.json");
    var oldInteraction = app.userInteractionLevel;
    var document = null;
    try {
        app.userInteractionLevel = UserInteractionLevel.DONTDISPLAYALERTS;
        if (!source.exists) throw new Error("Missing vector master: " + source.fsName);
        for (var i = app.documents.length - 1; i >= 0; --i) {
            var openDocument = app.documents[i];
            if (openDocument.name === "Congregation.svg" && openDocument.fullName.fsName === source.fsName) {
                if (!openDocument.saved) throw new Error("Save the modified SVG before exporting.");
                openDocument.close(SaveOptions.DONOTSAVECHANGES);
            } else if (openDocument.name === "Congregation.ai") {
                throw new Error("Close Congregation.ai before rebuilding it from SVG.");
            }
        }
        document = app.open(source, DocumentColorSpace.RGB);
        document.rulerUnits = RulerUnits.Pixels;
        var bounds = document.artboards[0].artboardRect;
        if (Math.abs(bounds[2] - bounds[0] - 1024) > 0.001 ||
            Math.abs(bounds[1] - bounds[3] - 1024) > 0.001)
            throw new Error("The vector artboard must be exactly 1024 by 1024 pixels.");
        document.artboards[0].name = "Congregation — 1024 px — square master";
        if (document.rasterItems.length || document.placedItems.length)
            throw new Error("The visible vector master must not contain raster artwork.");
        var paths = document.pathItems.length;
        var gradientFills = 0;
        for (var p = 0; p < document.pathItems.length; ++p) {
            var path = document.pathItems[p];
            if (path.filled && path.fillColor.typename === "GradientColor") ++gradientFills;
        }
        document.layers[0].name = "VECTOR — Congregation";

        // Keep the supplied sample on a hidden, nonprinting template layer, away from exports.
        var reference = new File(evidence.fsName + "/Reference.png");
        if (reference.exists) {
            var layer = document.layers.add();
            layer.name = "REFERENCE — supplied sample — hidden, nonprinting";
            var placed = layer.placedItems.add();
            placed.file = reference;
            placed.width = 1024;
            placed.height = 1024;
            placed.position = [bounds[0], bounds[1]];
            placed.embed();
            document.selection = null;
            layer.printable = false;
            layer.visible = false;
            layer.locked = true;
        }
        document.activeLayer = document.layers[document.layers.length - 1];
        var save = new IllustratorSaveOptions();
        save.pdfCompatible = true;
        save.compressed = true;
        save.embedICCProfile = true;
        document.saveAs(ai, save);
        var options = new ExportOptionsPNG24();
        options.antiAliasing = true;
        options.artBoardClipping = true;
        options.transparency = false;
        options.horizontalScale = 100;
        options.verticalScale = 100;
        document.exportFile(png, ExportType.PNG24, options);
        app.executeMenuCommand("fitall");
        document.save();
        report.encoding = "UTF-8";
        report.open("w");
        report.write('{"application":"Adobe Illustrator","version":"' + app.version +
            '","width":1024,"height":1024,"vector_paths":' + paths +
            ',"gradient_fills":' + gradientFills + ',"visible_rasters":0,"reference_hidden":true,' +
            '"ai_saved":' + ai.exists + ',"png_exported":' + png.exists + '}\n');
        report.close();
    } catch (error) {
        var failure = new File(evidence.fsName + "/illustrator-error.txt");
        failure.open("w");
        failure.write(error.toString() + "\nLine: " + error.line);
        failure.close();
        alert("Congregation icon export failed: " + error.toString());
    } finally {
        app.userInteractionLevel = oldInteraction;
    }
}());
