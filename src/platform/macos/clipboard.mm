/**
 * @file src/platform/macos/clipboard.mm
 * @brief Clipboard image access for macOS.
 */
// standard includes
#include <cstdint>
#include <optional>
#include <string>

// platform includes
#import <AppKit/AppKit.h>

// local includes
#include "src/platform/common.h"

namespace platf {
  std::optional<std::uint64_t> clipboard_change_count() {
    @autoreleasepool {
      return static_cast<std::uint64_t>([NSPasteboard generalPasteboard].changeCount);
    }
  }

  std::optional<std::string> get_clipboard_image_png(const std::size_t max_size) {
    @autoreleasepool {
      NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
      NSData *png = [pasteboard dataForType:NSPasteboardTypePNG];
      if (!png) {
        NSData *tiff = [pasteboard dataForType:NSPasteboardTypeTIFF];
        NSBitmapImageRep *rep = tiff ? [NSBitmapImageRep imageRepWithData:tiff] : nil;
        png = rep ? [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}] : nil;
      }
      if (!png || png.length == 0 || png.length > max_size) {
        return std::nullopt;
      }
      return std::string {static_cast<const char *>(png.bytes), static_cast<std::size_t>(png.length)};
    }
  }

  bool set_clipboard_image_png(const std::string &png) {
    @autoreleasepool {
      NSData *data = [NSData dataWithBytes:png.data() length:png.size()];
      NSBitmapImageRep *rep = [NSBitmapImageRep imageRepWithData:data];
      if (!rep) {
        return false;
      }

      // Some apps only read TIFF, so offer both representations.
      NSData *tiff = [rep TIFFRepresentation];
      NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
      [pasteboard clearContents];
      [pasteboard declareTypes:(tiff ? @[NSPasteboardTypePNG, NSPasteboardTypeTIFF] : @[NSPasteboardTypePNG]) owner:nil];
      return [pasteboard setData:data forType:NSPasteboardTypePNG] &&
             (!tiff || [pasteboard setData:tiff forType:NSPasteboardTypeTIFF]);
    }
  }
}  // namespace platf
