#ifndef MB_H
#define MB_H

#include <stdint.h>

/* multiboot2 boot information header (comes right after GRUB's %ebx/%rsi) */
struct mb_info
{
  uint32_t total_size;
  uint32_t reserved;
  /* tags follow, each 8-byte aligned */
};

/* generic tag header -- every tag starts with this */
struct mb_tag
{
  uint32_t type;
  uint32_t size;
};

#define MB_TAG_TYPE_END 0

/* memory map */

#define MB_TAG_TYPE_MEMORY_MAP 6
#define MB_TAG_TYPE_FRAMEBUFFER 8
#define MB_TAG_TYPE_ACPI_OLD 14
#define MB_TAG_TYPE_ACPI_NEW 15

/* payload of a type-8 (framebuffer) tag, immediately after struct mb_tag */
struct mb_tag_framebuffer
{
  struct mb_tag tag;
  uint64_t addr;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  uint8_t bpp;
  uint8_t fb_type;
  uint16_t reserved;
  /* color_info follows here, variable length depending on fb_type
     not modeled yet, add if/when you need palette or masked-RGB info */
};

struct framebuffer
{
  uint64_t addr;
  uint32_t pitch;
  uint32_t width;
  uint32_t height;
  uint8_t bpp;
  uint8_t fb_type;
  int present; /* 0 if no framebuffer tag was found */
};

/* walks the mb_info tag list, fills out fb. fb->present = 0 if not found. */
void mb_parse(struct mb_info *info, struct framebuffer *fb);

/* returns pointer to raw RSDP bytes (ACPI 1.0 or 2.0+), or NULL if absent.
 * prefers the 2.0+ (type 15) tag over 1.0 (type 14) when both exist -
 * the 2.0 structure is a superset (adds xsdt_address, a 64-bit pointer),
 * so it's strictly more useful and lets you skip the 32-bit RSDT path. */
void *mb_find_acpi_rsdp(struct mb_info *info);
struct multiboot_mmap_entry
{
  uint64_t addr;
  uint64_t len;
  uint32_t type;
  uint32_t zero;
};
struct multiboot_tag_mmap
{
  uint32_t type;
  uint32_t size;
  uint32_t entry_size;
  uint32_t entry_version;
  struct multiboot_mmap_entry entries[];
};

void parse_mmap(struct multiboot_tag_mmap *mmap);

#endif /* MB_H */
