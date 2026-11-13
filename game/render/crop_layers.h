// game/render/crop_layers.h — the packets of a Tomba! 2 draw that crop widescreen (psxport plan 0203) shows as UI,
// at their 4:3 size and place over the cropped world, and among them the text fonts' glyphs psxport draws from
// outline fonts (plan 0207, ui/text_fonts.h).
//
// A UI draw is one of the natives declared through uiDraw (the HUD gauge, the font, dialog and panel emitters, the
// UI sprite writers, the page step, the narration crawl): every packet it takes from the bump pool is UI. The pass's
// packets are named from the commit that seals the last pass to the next. A replay of the draw section takes the
// pool bytes the frame's own draw takes after it, so its packets are named apart, from its start to that draw; it
// also links the pass's update-block packets, which lie below it. The text drawers name each glyph packet they link
// with its font, kept by the same pass and replay.
#pragma once

#include "crop_ui_layers.h"
#include "native_function.h"
#include "outline_text.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

class Core;

namespace tomba::render {

class CropLayers final : public psx::present::CropUiLayers, public psx::present::OutlineGlyphs {
public:
  // The last pass's record is sealed: its packets are forgotten.
  void beginPass();
  // A replay of the draw section begins; its packets are named, and asked of, until endReplays.
  void beginReplay();
  // The frame's own draw follows its replays.
  void endReplays();
  [[nodiscard]] bool uiPacket(std::uint32_t packet) const override;
  // A text drawer linked the packet at `packet`, a glyph of text_fonts::Font `font`.
  void glyph(std::uint32_t packet, std::uint8_t font);
  [[nodiscard]] std::optional<std::uint8_t> glyphFont(std::uint32_t packet) const override;

  // The packets the pool hands out while one lives are UI; nested scopes name the outermost's.
  class Scope {
  public:
    explicit Scope(Core &core);
    ~Scope();
    Scope(const Scope &) = delete;
    Scope &operator=(const Scope &) = delete;
    Scope(Scope &&) = delete;
    Scope &operator=(Scope &&) = delete;

  private:
    Core &core_;
    std::uint32_t begin_;
  };

private:
  struct Range {
    std::uint32_t begin = 0; // main-RAM offsets, end exclusive
    std::uint32_t end = 0;
  };

  void name(std::uint32_t begin, std::uint32_t end);

  // A pass's or a replay's packets.
  struct Named {
    std::vector<Range> ui;
    std::vector<std::pair<std::uint32_t, std::uint8_t>> glyphs; // main-RAM offset and font
  };

  [[nodiscard]] Named &naming() {
    return replaying_ ? replay_ : pass_;
  }

  Named pass_;
  Named replay_;
  bool replaying_ = false;
  int depth_ = 0;
};

void uiDrawScoped(Core *core, psx::cpu::NativeFunction body);

// A native whose packets are UI.
template <psx::cpu::NativeFunction Body> void uiDraw(Core *core) {
  uiDrawScoped(core, Body);
}

} // namespace tomba::render
