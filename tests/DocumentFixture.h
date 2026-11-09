#pragma once
#include <Native/Artwork.h>
#include <iiSharedCanvasNative.h>
#include <filesystem>
#include <utility>
// Retired infinite documents are decoded only as SDK fixtures. Product open rejects them.
inline bool open_document_fixture(congregation::native::Artwork& artwork, const std::filesystem::path& path)
{
    iiSharedCanvas::Document document;
    const auto read = iiSharedCanvas::DocumentFile::readSnapshot(path.string(), document);
    if (!read.ok()) return false;
    if (document.canvasMode != iiSharedCanvas::CanvasMode::Infinite) return artwork.open(path);
    if (artwork.open(path)) return false;
    return artwork.replace(std::move(document));
}
