#include "../include/mb.h"
void mb_parse(struct mb_info *info, struct framebuffer *fb) {
  fb->present = 0;
  uint8_t *ptr = (uint8_t *)info + sizeof(struct mb_info);
  uint8_t *end = (uint8_t *)info + info->total_size;
  while (ptr < end) {
    struct mb_tag *tag = (struct mb_tag *)ptr;

    if (tag->type == MB_TAG_TYPE_END) {
      break;
    }

    if (tag->type == MB_TAG_TYPE_FRAMEBUFFER) {
      struct mb_tag_framebuffer *fbt = (struct mb_tag_framebuffer *)tag;
      fb->addr = fbt->addr;
      fb->pitch = fbt->pitch;
      fb->width = fbt->width;
      fb->height = fbt->height;
      fb->bpp = fbt->bpp;
      fb->fb_type = fbt->fb_type;
      fb->present = 1;
    } else if (tag->type == MB_TAG_TYPE_MEMORY_MAP) {
      parse_mmap((struct multiboot_tag_mmap *)tag);
    }
    /* each tag is padded to 8-byte alignment before the next one */
    uint32_t advance = (tag->size + 7) & ~7u;
    ptr += advance;
  }
}
void *mb_find_acpi_rsdp(struct mb_info *info) {
  uint8_t *ptr = (uint8_t *)info + sizeof(struct mb_info);
  uint8_t *end = (uint8_t *)info + info->total_size;
  void *old_rsdp = 0;

  while (ptr < end) {
    struct mb_tag *tag = (struct mb_tag *)ptr;
    if (tag->type == MB_TAG_TYPE_END)
      break;
    if (tag->type == MB_TAG_TYPE_ACPI_NEW)
      return (void *)(tag + 1);
    if (tag->type == MB_TAG_TYPE_ACPI_OLD)
      old_rsdp = (void *)(tag + 1);
    uint32_t adv = (tag->size + 7) & ~7u;
    ptr += adv;
  }
  return old_rsdp; // NULL if either tag was never found.
}