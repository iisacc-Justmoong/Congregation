#include <Canvas/Artwork.h>
#include <Document/LayerCreation.h>
#include <iiFileProvider>
#include <iiSharedCanvas>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

using namespace congregation::native;
using namespace iiSharedCanvas;
namespace {
void check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
Document source_document(Document d) {
    const auto add = [&](Asset asset, Layer layer) {
        const auto id = assetId(asset);
        layerProperties(layer) = {"layer-" + id, id};
        setLayerSource(layer, StaticSource{id});
        d.assets.push_back(std::move(asset)); d.layers.push_back(std::move(layer));
    };
    add(VideoAsset{"video", {24, 1}, {makeRasterLayer(2, 2, 0xff123456U)}}, VideoLayer{});
    SemanticSegmentLayer semantic;
    semantic.segmentation.taxonomy.id = "classes";
    SemanticClass category; category.id = 1; category.key = "person"; category.name = "Person";
    semantic.segmentation.taxonomy.classes = {category};
    add(RasterAsset{"semantic", makeRasterLayer(2, 2, 0xff000000U)}, semantic);
    add(PoseAsset{"pose", {2, 2}, {makeNeutralPosePerson("person")}}, PoseLayer{});
    add(DepthAsset{"depth", {2, 2}, {0, .2, .5, 1}}, DepthLayer{});
    add(LineArtAsset{"line-art", {2, 2}, {0, .2, .5, 1}}, LineArtLayer{});
    add(CannyAsset{"canny", {2, 2}, {0, 1, 0, 1}}, CannyLayer{});
    add(ScribbleAsset{"scribble", {2, 2}, {1, 0, 1, 0}}, ScribbleLayer{});
    add(MlsdAsset{"mlsd", {2, 2}, {{"segment", 0, 0, 1, 1, 1, true}}}, MlsdLayer{});
    add(NormalMapAsset{"normal", {2, 2}, std::vector<NormalMapSample>(4)}, NormalMapLayer{});
    add(ShuffleAsset{"shuffle", {2, 2}, std::vector<ShuffleColor>(4)}, ShuffleLayer{});
    add(TileAsset{"tile", {2, 2}, std::vector<TileColor>(4)}, TileLayer{});
    add(ReferenceAsset{"reference", {2, 2}, std::vector<ReferenceColor>(4)}, ReferenceLayer{});
    IpAdapterAsset ip; ip.id = "ip";
    ip.descriptor = {IpAdapterEmbeddingStage::ProjectedTokens, "encoder", "e1", "adapter", "a1", "sdxl", "crop224"};
    ip.conditional = {1, 2, {1, -1}}; ip.unconditional = IpAdapterTensor{1, 2, {.25F, .5F}};
    IpAdapterLayer adapter; adapter.control.modelId = "adapter"; adapter.control.modelRevision = "a1";
    add(std::move(ip), std::move(adapter));
    d.assets.emplace_back(RasterAsset{"extra-mask", makeRasterLayer(2, 2, 0xffffffffU)});
    iiSharedCanvas::LayerMask mask; mask.assetId = "extra-mask";
    layerProperties(*findLayer(d, "layer-video")).appearance.rasterMask = mask;
    const auto validation = validate(d);
    if (!validation.ok()) throw std::runtime_error("invalid source fixture: " + validation.issues.front().path + ": " + validation.issues.front().message);
    return d;
}
}
int main(int argc, char** argv) try {
    check(argc == 2, "test resource directory required");
    iiFileProvider::native::TemporaryDirectory temporary(argv[1]);
    check(temporary.isValid(), "cannot create isolated resources");
    std::set<std::string_view> keys;
    for (const auto& option : new_layer_options())
        check(keys.insert(option.id).second, "duplicate layer menu identity");
    check(keys.size() == 18, "layer menu does not cover the SDK layer kinds and audio track");

    constexpr std::size_t budget = 768 * 1024;
    Artwork artwork(temporary.path(), budget);
    check(artwork.create(512, 512), "initial canvas failed");
    auto document = artwork.document();
    auto* pixels = findRasterAsset(document, "paint");
    check(pixels != nullptr, "fixture bitmap not found");
    std::uint32_t random = 7;
    for (auto& pixel : pixels->pixels.pixels) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        pixel = 0xff000000U | (random & 0xffffffU);
    }
    check(artwork.replace(std::move(document)) && artwork.snapshot().size() > budget / 2,
          "fixture does not exceed snapshot history budget");
    for (const auto kind : {NewLayerType::raster, NewLayerType::vector,
                            NewLayerType::animated_raster, NewLayerType::animated_vector}) {
        const auto before = artwork.snapshot();
        const auto previous = artwork.selected();
        const auto previous_frames = artwork.document().timeline.frameCount;
        if (!artwork.add_layer(kind)) throw std::runtime_error("layer creation failed: " + artwork.error());
        const auto* layer = findLayer(artwork.document(), artwork.selected());
        check(layer != nullptr, "new layer was not selected");
        const bool is_bitmap = kind == NewLayerType::raster || kind == NewLayerType::animated_raster;
        const bool animated = kind == NewLayerType::animated_raster || kind == NewLayerType::animated_vector;
        check(layerRepresentation(*layer) == (is_bitmap ? LayerRepresentation::Bitmap : LayerRepresentation::Vector) &&
              layerTiming(*layer) == (animated ? LayerTiming::Dynamic : LayerTiming::Static), "wrong native layer kind");
        check(resolveAssetAt(artwork.document(), *layer, 0) != nullptr, "layer has no frame-zero content");
        if (animated) {
            const auto* keys = keyframedLayerSource(*layer);
            check(layerProperties(*layer).frameRange == LayerFrameRange{0, 119} &&
                  artwork.document().timeline.frameCount == 120 && keys &&
                  keys->frameIndices == std::vector<FrameIndex>{0} &&
                  resolveAssetAt(artwork.document(), *layer, 119) == resolveAssetAt(artwork.document(), *layer, 0),
                  "an animated layer must have five seconds of frames and one shared initial asset");
        } else {
            check(artwork.document().timeline.frameCount == previous_frames &&
                  staticLayerTimelineRange(artwork.document(), *layer) == LayerFrameRange{0, 0},
                  "a static layer must occupy one frame without extending the timeline");
        }
        const auto after = artwork.snapshot();
        check(artwork.history_bytes() <= budget && artwork.undo() && artwork.selected() == previous &&
              artwork.snapshot() == before && artwork.redo() && artwork.snapshot() == after,
              "layer creation undo/redo changed the document");
    }
    if (!artwork.add_layer(NewLayerType::audio))
        throw std::runtime_error("audio track creation failed: " + artwork.error());
    const auto audio_id = artwork.selected();
    check(findAudioTrack(artwork.document(), audio_id) && artwork.undo() &&
          !findAudioTrack(artwork.document(), audio_id) && artwork.redo() &&
          findAudioTrack(artwork.document(), audio_id), "audio track history failed");
    const auto output = temporary.path() / "layers.iisc";
    check(artwork.save(output), "save failed");
    Artwork reopened(temporary.path());
    check(reopened.open(output) && reopened.document().layers.size() == artwork.document().layers.size() &&
          findAudioTrack(reopened.document(), audio_id) && reopened.snapshot() == artwork.snapshot(),
          "native layers and their timing did not reopen");

    Artwork fractional(temporary.path());
    check(fractional.create(8, 8), "cannot create the fractional timeline fixture");
    auto fractional_document = fractional.document();
    fractional_document.timeline = {{25, 2}, 80};
    check(fractional.replace(std::move(fractional_document)) &&
          fractional.add_layer(NewLayerType::animated_vector), "cannot create a fractional-rate dynamic layer");
    const auto* fractional_layer = findLayer(fractional.document(), fractional.selected());
    check(fractional_layer && layerProperties(*fractional_layer).frameRange == LayerFrameRange{0, 62} &&
          fractional.document().timeline.frameCount == 80 &&
          fractional.document().timeline.frameRate.numerator == 25 &&
          fractional.document().timeline.frameRate.denominator == 2 &&
          resolveAssetAt(fractional.document(), *fractional_layer, 62) &&
          !resolveAssetAt(fractional.document(), *fractional_layer, 63),
          "a five-second layer must round up at a fractional rate and preserve a longer document");

    Artwork source(temporary.path());
    check(source.create(2, 2) && source.replace(source_document(source.document())), "cannot create source fixture");
    const auto source_path = temporary.path() / "source-layers.iisc";
    check(source.save(source_path), "cannot save native source fixture");
    Artwork target(temporary.path());
    check(target.create(2, 2), "cannot create import target");
    const std::map<NewLayerType, std::string_view> source_ids{
        {NewLayerType::video, "video"}, {NewLayerType::semantic_segment, "semantic"},
        {NewLayerType::pose, "pose"}, {NewLayerType::depth, "depth"},
        {NewLayerType::line_art, "line-art"}, {NewLayerType::canny, "canny"},
        {NewLayerType::scribble, "scribble"}, {NewLayerType::mlsd, "mlsd"},
        {NewLayerType::normal_map, "normal"}, {NewLayerType::shuffle, "shuffle"},
        {NewLayerType::tile, "tile"}, {NewLayerType::reference, "reference"},
        {NewLayerType::ip_adapter, "ip"}
    };
    for (const auto& option : new_layer_options()) {
        if (option.source != LayerSourceRequirement::native_layer) continue;
        const auto before = target.snapshot();
        if (!target.add_layer_from_file(option.type, source_path))
            throw std::runtime_error("source import " + std::string(option.id) + ": " + target.error());
        const auto* imported = findLayer(target.document(), target.selected());
        check(imported != nullptr, "import did not select its layer");
        const auto* content = resolveAssetAt(target.document(), *imported, 0);
        const auto original_id = std::string(source_ids.at(option.type));
        const auto* original = findLayer(source.document(), "layer-" + original_id);
        const auto* original_content = original ? resolveAssetAt(source.document(), *original, 0) : nullptr;
        check(content && original_content && imported->index() == original->index() &&
              content->index() == original_content->index() && assetId(*content) != original_id,
              "import changed a layer kind or reused a source asset identity");
        if (option.type == NewLayerType::video) {
            const auto* video = std::get_if<VideoAsset>(content);
            check(video && video->frames.at(0).pixels.at(0) == 0xff123456U, "video frames changed during import");
        } else {
            check(controlNetKind(*imported).has_value(), "conditioning layer lost its type");
        }
        if (option.type == NewLayerType::video) {
            const auto& mask = layerProperties(*imported).appearance.rasterMask;
            check(mask && mask->assetId != "extra-mask" && findRasterAsset(target.document(), mask->assetId),
                  "import did not preserve and remap the layer mask");
        }
        if (option.type == NewLayerType::ip_adapter) {
            const auto* ip = std::get_if<IpAdapterAsset>(content);
            check(ip && ip->descriptor.encoderId == "encoder" && ip->conditional.values.at(1) == -1 && ip->unconditional,
                  "IP-Adapter data or provenance changed during import");
        }
        const auto after = target.snapshot();
        check(target.undo() && target.snapshot() == before && target.redo() && target.snapshot() == after,
              "import undo/redo changed document data");
    }
    const auto imported_path = temporary.path() / "imported.iisc";
    check(target.save(imported_path), "cannot save imported layers");
    Artwork imported(temporary.path());
    check(imported.open(imported_path) && imported.snapshot() == target.snapshot(), "imported layers did not reopen");
    const auto before_bad_import = target.snapshot();
    check(!target.add_layer_from_file(NewLayerType::pose, output) && target.snapshot() == before_bad_import,
          "a source document without the requested type must not alter the target");
    // History charges the imported layer and assets independently of existing pixels.
    const auto large_before = artwork.snapshot();
    const auto large_selection = artwork.selected();
    check(artwork.add_layer_from_file(NewLayerType::video, source_path) && artwork.history_bytes() <= budget,
          "a small import into a large document must use incremental history");
    check(artwork.undo() && artwork.snapshot() == large_before && artwork.selected() == large_selection,
          "undoing the small import must restore the large document and selection");

    Artwork over_budget(temporary.path(), 1);
    check(over_budget.create(2, 2), "cannot create the budget-limited import target");
    const auto limited_before = over_budget.snapshot();
    const auto limited_selection = over_budget.selected();
    check(!over_budget.add_layer_from_file(NewLayerType::video, source_path) &&
          over_budget.error() == "Edit exceeds the undo budget" &&
          over_budget.snapshot() == limited_before && over_budget.selected() == limited_selection &&
          over_budget.history_bytes() == 0,
          "over-budget imports must leave the document intact");

    auto animated_document = source.document();
    animated_document.timeline.frameCount = 2;
    animated_document.assets.emplace_back(CannyAsset{"canny-next", {2, 2}, {1, 1, 0, 0}});
    auto* original_canny = findLayer(animated_document, "layer-canny");
    check(original_canny != nullptr, "cannot find animated source");
    setLayerSource(*original_canny, KeyframedSource{{0, 1}});
    animated_document.frames = {{0, {{"layer-canny", "canny"}}}, {1, {{"layer-canny", "canny-next"}}}};
    check(validate(animated_document).ok(), "invalid animated source fixture");
    Artwork animated_source(temporary.path());
    check(animated_source.create(2, 2) && animated_source.replace(std::move(animated_document)), "cannot stage animated source");
    const auto animated_path = temporary.path() / "animated.iisc";
    check(animated_source.save(animated_path), "cannot save animated source");
    const auto before_short = target.snapshot();
    check(!target.add_layer_from_file(NewLayerType::canny, animated_path) && target.snapshot() == before_short,
          "out-of-range keyframes must be rejected without truncation");
    check(target.timing(24, 2) && target.add_layer_from_file(NewLayerType::canny, animated_path),
          "cannot import keyframed conditioning layer");
    const auto* animated = findLayer(target.document(), target.selected());
    const auto* first = animated ? resolveAssetAt(target.document(), *animated, 0) : nullptr;
    const auto* second = animated ? resolveAssetAt(target.document(), *animated, 1) : nullptr;
    check(animated && layerTiming(*animated) == LayerTiming::Dynamic && first && second &&
          std::get<CannyAsset>(*first).mask == std::vector<std::uint8_t>({0, 1, 0, 1}) &&
          std::get<CannyAsset>(*second).mask == std::vector<std::uint8_t>({1, 1, 0, 0}),
          "animated conditioning frames changed during import");
    const auto animated_after = target.snapshot();
    check(target.undo() && target.redo() && target.snapshot() == animated_after, "animated import undo/redo failed");
    check(target.add_layer(NewLayerType::audio), "cannot add audio lifecycle fixture");
    const auto track = target.selected();
    check(target.rename(track, "Narration") && target.visible(track, false) &&
          findAudioTrack(target.document(), track)->name == "Narration" &&
          findAudioTrack(target.document(), track)->muted, "audio name or mute did not persist");
    const auto muted = target.snapshot();
    check(target.undo() && !findAudioTrack(target.document(), track)->muted &&
          target.redo() && target.snapshot() == muted, "audio mute undo/redo failed");
    check(target.remove_layer() && !findAudioTrack(target.document(), track) && target.undo() &&
          findAudioTrack(target.document(), track), "audio delete undo failed");

    const auto path = temporary.path() / "working", moved = temporary.path() / "working-moved";
    std::filesystem::create_directory(path);
    Artwork failing(path, budget);
    check(failing.create(32, 32) && failing.add_layer(NewLayerType::vector) && failing.undo() && failing.can_redo(),
          "failure fixture failed");
    const auto prior = failing.snapshot();
    std::filesystem::rename(path, moved);
    const bool rejected = !failing.add_layer(NewLayerType::animated_raster);
    std::filesystem::rename(moved, path);
    check(rejected && failing.snapshot() == prior && failing.can_redo() && failing.redo(),
          "failed layer creation damaged the document or redo history");
    std::cout << "NEW_LAYER_INTEGRATION_OK\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
