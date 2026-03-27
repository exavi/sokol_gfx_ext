#if defined(SOKOL_IMPL) && !defined(SOKOL_GFX_EXT_PARTIAL_UPDATE_IMPL)
#define SOKOL_GFX_EXT_PARTIAL_UPDATE_IMPL
#endif
#ifndef SOKOL_GFX_EXT_PARTIAL_UPDATE_INCLUDED
/*
    sokol_gfx_ext_partial_update.h -- partial texture update extension for sokol_gfx

    Provides partial texture update support for sokol_gfx, which normally only
    supports full texture updates.

    USAGE:
        Do this:
            #define SOKOL_IMPL or
            #define SOKOL_GFX_EXT_PARTIAL_UPDATE_IMPL
        before you include this file in *one* C or C++ file to create the
        implementation.

        In the same place define one of the following to select the rendering
        backend:
            #define SOKOL_GLCORE
            #define SOKOL_GLES3
            #define SOKOL_METAL
            #define SOKOL_VULKAN

    EXAMPLE:
        // Enable queued updates for a texture
        sgext_enable_image_queue(my_texture, true);

        // Queue a partial update
        sgext_image_region region = {
            .x_start = 0, .y_start = 0,
            .width = 256, .height = 256
        };
        sg_image_data data = { ... };
        sgext_queue_image_update(my_texture, &region, &data, SGEXT_OWNERSHIP_COPY);

        // Before sg_apply_bindings, apply queued updates
        sg_bindings bindings = { ... };
        sgext_apply_image_updates(&bindings);
        sg_apply_bindings(&bindings);
*/
#define SOKOL_GFX_EXT_PARTIAL_UPDATE_INCLUDED (1)

#if !defined(SOKOL_GFX_INCLUDED)
#error "Please include sokol_gfx.h before sokol_gfx_ext_partial_update.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
    sgext_ownership

    Defines how partial update data ownership is handled.
*/
typedef enum sgext_ownership {
    _SGEXT_OWNERSHIP_DEFAULT,     // value 0 reserved for default-init
    SGEXT_OWNERSHIP_REFERENCE,    // default: references data directly (expects +SG_NUM_INFLIGHT_FRAMES lifetime)
    SGEXT_OWNERSHIP_MOVE,         // move: takes ownership of pointers given
    SGEXT_OWNERSHIP_COPY,         // copy: copies data into temp memory
    _SGEXT_OWNERSHIP_NUM,
    _SGEXT_OWNERSHIP_FORCE_U32 = 0x7FFFFFFF
} sgext_ownership;

/*
    sgext_image_region

    Defines a rectangular region within a texture for partial updates.
*/
typedef struct sgext_image_region {
    uint32_t _start_canary;
    int x_start;
    int y_start;
    int z_start;
    int width;
    int height;
    int depth;
    uint32_t _end_canary;
} sgext_image_region;

/*
    sgext_update_image_region

    Immediately update a region of a texture.

    (this function might disappear in the future, it's generally better to enqueue updates)

    NOTE: advance_active_slot should be generally true, only use false if you really know what you're doing.

    Parameters:
        img: The texture to update
        region: The region to update
        data: The image data (same format as sg_image_data)
        advance_active_slot: If true, advances to next slot for multi-slot textures
*/
SOKOL_GFX_API_DECL void sgext_update_image_region(sg_image img, const sgext_image_region* region, const sg_image_data* data, bool advance_active_slot);

/*
    sgext_apply_image_updates

    Apply all queued partial updates for textures in the bindings.
    Call this BEFORE sg_apply_bindings().

    Parameters:
        bindings: The bindings that will be applied
*/
SOKOL_GFX_API_DECL void sgext_apply_image_updates(const sg_bindings* bindings);

/*
    sgext_enable_image_queue

    Enable or disable queued partial updates for a texture.

    Parameters:
        img: The texture
        enabled: true to enable queued updates, false to disable
*/
SOKOL_GFX_API_DECL void sgext_enable_image_queue(sg_image img, bool enabled);

/*
    sgext_queue_image_update

    Queue a partial update for a texture. The 'real' update will be applied on 
    sgext_apply_image_updates() before the texture is bound, across all inflight frames.

    NOTE: Image must have queued updates enabled via sgext_enable_image_queue()

    Parameters:
        img: The texture to update
        region: The region to update
        data: The image data
        ownership: How to handle the data memory
*/
SOKOL_GFX_API_DECL void sgext_queue_image_update(sg_image img, const sgext_image_region* region, const sg_image_data* data, sgext_ownership ownership);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // SOKOL_GFX_EXT_PARTIAL_UPDATE_INCLUDED

// ██ ███    ███ ██████  ██      ███████ ███    ███ ███████ ███    ██ ████████  █████  ████████ ██  ██████  ███    ██
// ██ ████  ████ ██   ██ ██      ██      ████  ████ ██      ████   ██    ██    ██   ██    ██    ██ ██    ██ ████   ██
// ██ ██ ████ ██ ██████  ██      █████   ██ ████ ██ █████   ██ ██  ██    ██    ███████    ██    ██ ██    ██ ██ ██  ██
// ██ ██  ██  ██ ██      ██      ██      ██  ██  ██ ██      ██  ██ ██    ██    ██   ██    ██    ██ ██    ██ ██  ██ ██
// ██ ██      ██ ██      ███████ ███████ ██      ██ ███████ ██   ████    ██    ██   ██    ██    ██  ██████  ██   ████
//
// >>implementation
#ifdef SOKOL_GFX_EXT_PARTIAL_UPDATE_IMPL
#define SOKOL_GFX_EXT_PARTIAL_UPDATE_IMPL_INCLUDED (1)

#ifndef SOKOL_GFX_IMPL_INCLUDED
#error "Please include sokol_gfx implementation before sokol_gfx_ext_partial_update.h implementation"
#endif

#ifndef __cplusplus
#error "sokol_gfx_ext_partial_update.h implementation requires C++"
#endif

#include <list>
#include <unordered_map>

#if defined(SOKOL_METAL)
#include <Metal/Metal.h>
#endif

// Internal structure to track queued partial updates
struct _sgext_image_update_info
{
    sgext_image_region region;
    sg_image_data data;
    uint32_t update_frame_count;
    sgext_ownership ownership;
};

// Global queue of pending partial updates, keyed by image ID
static std::unordered_map<uint32_t, std::list<_sgext_image_update_info>> g_image_update_queue;

// Helper to copy or reference image data
_SOKOL_PRIVATE void _sgext_copy_image_data_or_fill(const sg_image_data& src, sg_image_data& dst, bool deep_copy)
{
    dst = src;

    if (deep_copy)
    {
        for (int mip = 0; mip < SG_MAX_MIPMAPS; mip++)
        {
            const sg_range& rg_src = src.mip_levels[mip];
            if (!rg_src.ptr || !rg_src.size)
                continue;

            void* ptr = malloc(rg_src.size);
            memcpy(ptr, rg_src.ptr, rg_src.size);

            sg_range& rg_dst = dst.mip_levels[mip];
            rg_dst.ptr = ptr;
            rg_dst.size = rg_src.size;
        }
    }
}

// Helper to free copied image data
_SOKOL_PRIVATE void _sgext_free_image_update_info(const _sgext_image_update_info& update_info)
{
    if (update_info.ownership == SGEXT_OWNERSHIP_REFERENCE)
        return;

    for (int mip = 0; mip < SG_MAX_MIPMAPS; mip++)
    {
        const sg_range& rg = update_info.data.mip_levels[mip];
        void* ptr = const_cast<void*>(rg.ptr);
        if (ptr && rg.size)
            free(ptr);
    }
}

// Backend-specific implementations
#if defined(_SOKOL_ANY_GL)

_SOKOL_PRIVATE void _sgext_gl_update_image_region(_sg_image_t* img, const sgext_image_region* upd_region, const sg_image_data* data, bool advance_active_slot) {
    SOKOL_ASSERT(img && data);
    if (img->cmn.upd_frame_index != _sg.frame_index && advance_active_slot)
    {
        if (++img->cmn.active_slot >= img->cmn.num_slots) {
            img->cmn.active_slot = 0;
        }
    }
    SOKOL_ASSERT(img->cmn.active_slot < SG_NUM_INFLIGHT_FRAMES);
    SOKOL_ASSERT(0 != img->gl.tex[img->cmn.active_slot]);
    _sg_gl_cache_store_texture_sampler_binding(0);
    _sg_gl_cache_bind_texture_sampler(0, img->gl.target, img->gl.tex[img->cmn.active_slot], 0);
    const GLenum gl_img_format = _sg_gl_teximage_format(img->cmn.pixel_format);
    const GLenum gl_img_type = _sg_gl_teximage_type(img->cmn.pixel_format);
    const int num_mips = img->cmn.num_mipmaps;
    for (int mip_index = 0; mip_index < num_mips; mip_index++) {
        const GLvoid* data_ptr = data->mip_levels[mip_index].ptr;
        if (!data_ptr) continue;

        if ((SG_IMAGETYPE_2D == img->cmn.type) || (SG_IMAGETYPE_CUBE == img->cmn.type)) {
            glTexSubImage2D(img->gl.target, mip_index,
                            upd_region->x_start, upd_region->y_start,
                            upd_region->width, upd_region->height,
                            gl_img_format, gl_img_type,
                            data_ptr);
        } else if ((SG_IMAGETYPE_3D == img->cmn.type) || (SG_IMAGETYPE_ARRAY == img->cmn.type)) {
            glTexSubImage3D(img->gl.target, mip_index,
                            upd_region->x_start, upd_region->y_start, upd_region->z_start,
                            upd_region->width, upd_region->height, upd_region->depth,
                            gl_img_format, gl_img_type,
                            data_ptr);
        }
    }
    _sg_gl_cache_restore_texture_sampler_binding(0);
}

#elif defined(SOKOL_METAL)

_SOKOL_PRIVATE void _sgext_mtl_copy_image_region_data(const _sg_image_t* img, __unsafe_unretained id<MTLTexture> mtl_tex, const sgext_image_region* upd_region, const sg_image_data* data)
{
    for (int mip_index = 0; mip_index < img->cmn.num_mipmaps; mip_index++) {
        if (!data->mip_levels[mip_index].ptr)
            continue;
        SOKOL_ASSERT(data->mip_levels[mip_index].size > 0);
        const uint8_t* data_ptr = (const uint8_t*)data->mip_levels[mip_index].ptr;
        int bytes_per_row = _sg_row_pitch(img->cmn.pixel_format, upd_region->width, 1);
        int bytes_per_slice = _sg_surface_pitch(img->cmn.pixel_format, upd_region->width, upd_region->height, 1);

        MTLRegion region;
        int bytes_per_image;
        if (img->cmn.type == SG_IMAGETYPE_3D) {
            region = MTLRegionMake3D(upd_region->x_start, upd_region->y_start, upd_region->z_start, (NSUInteger)upd_region->width, (NSUInteger)upd_region->height, (NSUInteger)upd_region->depth);
            bytes_per_image = bytes_per_slice;
        } else {
            region = MTLRegionMake2D(upd_region->x_start, upd_region->y_start, (NSUInteger)upd_region->width, (NSUInteger)upd_region->height);
            bytes_per_image = 0;
        }

        if (mtl_tex.storageMode == MTLStorageModePrivate)
        {
            // MTLTexture replaceRegion: can't be used on private storage
            // mode textures so we must create a temporary texture and blit it
            MTLTextureDescriptor* tmp_desc = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:mtl_tex.pixelFormat
                                                                                                width:upd_region->width
                                                                                               height:upd_region->height
                                                                                              mipmapped:NO];
            tmp_desc.storageMode = MTLStorageModeShared;
            id<MTLTexture> tmp_mtl_tex = [(__bridge id<MTLDevice>)_sg.mtl.device newTextureWithDescriptor:tmp_desc];

            // Upload CPU data to temp texture
            [tmp_mtl_tex replaceRegion:MTLRegionMake2D(0, 0, upd_region->width, upd_region->height)
                           mipmapLevel:0
                             withBytes:data_ptr
                           bytesPerRow:bytes_per_row];

            // Blit from temp texture to private texture
            id<MTLBlitCommandEncoder> blitEncoder = [(__bridge id<MTLCommandBuffer>)_sg.mtl.cmd_buffer blitCommandEncoder];
            [blitEncoder copyFromTexture:tmp_mtl_tex
                             sourceSlice:0
                             sourceLevel:0
                            sourceOrigin:MTLOriginMake(0, 0, 0)
                              sourceSize:MTLSizeMake((NSUInteger)upd_region->width, (NSUInteger)upd_region->height, 1)
                               toTexture:mtl_tex
                        destinationSlice:0
                        destinationLevel:mip_index
                       destinationOrigin:MTLOriginMake((NSUInteger)upd_region->x_start, (NSUInteger)upd_region->y_start, 0)];
            [blitEncoder endEncoding];
        }
        else
        {
            [mtl_tex replaceRegion:region
                       mipmapLevel:(NSUInteger)mip_index
                             slice:0
                         withBytes:data_ptr
                       bytesPerRow:(NSUInteger)bytes_per_row
                     bytesPerImage:(NSUInteger)bytes_per_image];
        }
    }
}

_SOKOL_PRIVATE void _sgext_mtl_update_image_region(_sg_image_t* img, const sgext_image_region* region, const sg_image_data* data, bool advance_active_slot)
{
    SOKOL_ASSERT(img && data);
    if (img->cmn.upd_frame_index != _sg.frame_index && advance_active_slot)
    {
        if (++img->cmn.active_slot >= img->cmn.num_slots) {
            img->cmn.active_slot = 0;
        }
    }
    __unsafe_unretained id<MTLTexture> mtl_tex = (__bridge id<MTLTexture>)_sg_mtl_id(img->mtl.tex[img->cmn.active_slot]);
    _sgext_mtl_copy_image_region_data(img, mtl_tex, region, data);
}

#elif defined(SOKOL_VULKAN)

_SOKOL_PRIVATE void _sgext_vk_update_image_region(_sg_image_t* img, const sgext_image_region* upd_region, const sg_image_data* data, bool advance_active_slot) {
    SOKOL_ASSERT(img && data);
    SOKOL_ASSERT(img->vk.img);

    // Advance the active slot if this is the first update this frame (mirrors GL/Metal behaviour)
    if (img->cmn.upd_frame_index != _sg.frame_index && advance_active_slot) {
        if (++img->cmn.active_slot >= img->cmn.num_slots) {
            img->cmn.active_slot = 0;
        }
    }

    // Ensure frame command buffers are acquired (idempotent within a frame)
    _sg_vk_acquire_frame_command_buffers();
    VkCommandBuffer cmd_buf = _sg.vk.frame.stream_cmd_buf;
    SOKOL_ASSERT(cmd_buf);

    // Transition image to transfer-destination layout
    _sg_vk_image_barrier(cmd_buf, img, _SG_VK_ACCESS_STAGING);

    const int num_mips = img->cmn.num_mipmaps;
    for (int mip_index = 0; mip_index < num_mips; mip_index++) {
        const sg_range* src_mip = &data->mip_levels[mip_index];
        if (!src_mip->ptr || src_mip->size == 0)
            continue;

        // Copy data into the per-frame stream staging buffer
        const uint32_t src_offset = (uint32_t)_sg_vk_shared_buffer_memcpy(
            &_sg.vk.stage.stream, src_mip->ptr, (uint32_t)src_mip->size);
        if (src_offset == _SG_VK_SHARED_BUFFER_OVERFLOW_RESULT) {
            _SG_ERROR(VULKAN_STAGING_STREAM_BUFFER_OVERFLOW);
            break;
        }

        // Determine aspect mask
        VkImageAspectFlags aspect_mask;
        if (_sg_is_depth_or_depth_stencil_format(img->cmn.pixel_format)) {
            aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (img->cmn.pixel_format == SG_PIXELFORMAT_DEPTH_STENCIL) {
                aspect_mask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        } else {
            aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        // Compute row pitch so bufferRowLength matches what was uploaded
        const int bytes_per_row = _sg_row_pitch(img->cmn.pixel_format, upd_region->width, 1);
        const int block_bytesize = _sg_block_bytesize(img->cmn.pixel_format);
        const int block_dim      = _sg_block_dim(img->cmn.pixel_format);
        // bufferRowLength and bufferImageHeight are in texels, not bytes
        const uint32_t buffer_row_length   = (uint32_t)((bytes_per_row / block_bytesize) * block_dim);
        const uint32_t buffer_image_height = (uint32_t)upd_region->height;

        // Number of depth/array slices for this mip
        int mip_slices;
        if (img->cmn.type == SG_IMAGETYPE_3D) {
            mip_slices = (upd_region->depth > 0) ? upd_region->depth : 1;
        } else {
            mip_slices = 1;
        }

        VkBufferImageCopy2 region = {};
        region.sType              = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
        region.bufferOffset       = src_offset;
        region.bufferRowLength    = buffer_row_length;
        region.bufferImageHeight  = buffer_image_height;
        region.imageSubresource.aspectMask     = aspect_mask;
        region.imageSubresource.mipLevel       = (uint32_t)mip_index;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = (img->cmn.type == SG_IMAGETYPE_3D) ? 1 : (uint32_t)mip_slices;
        region.imageOffset.x      = (int32_t)upd_region->x_start;
        region.imageOffset.y      = (int32_t)upd_region->y_start;
        region.imageOffset.z      = (img->cmn.type == SG_IMAGETYPE_3D) ? (int32_t)upd_region->z_start : 0;
        region.imageExtent.width  = (uint32_t)upd_region->width;
        region.imageExtent.height = (uint32_t)upd_region->height;
        region.imageExtent.depth  = (img->cmn.type == SG_IMAGETYPE_3D) ? (uint32_t)mip_slices : 1;

        VkCopyBufferToImageInfo2 copy_info = {};
        copy_info.sType          = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
        copy_info.srcBuffer      = _sg.vk.stage.stream.cur_buf;
        copy_info.dstImage       = (VkImage)img->vk.img;
        copy_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        copy_info.regionCount    = 1;
        copy_info.pRegions       = &region;

        vkCmdCopyBufferToImage2(cmd_buf, &copy_info);
    }

    // Transition image back to shader-read layout
    _sg_vk_image_barrier(cmd_buf, img, _SG_VK_ACCESS_TEXTURE);
}

#endif

// Internal function to apply a single partial update
_SOKOL_PRIVATE void _sgext_update_image_region_internal(_sg_image_t* img, const sgext_image_region* region, const sg_image_data* data, bool advance_active_slot)
{
#if defined(_SOKOL_ANY_GL)
    _sgext_gl_update_image_region(img, region, data, advance_active_slot);
#elif defined(SOKOL_METAL)
    _sgext_mtl_update_image_region(img, region, data, advance_active_slot);
#elif defined(SOKOL_VULKAN)
    _sgext_vk_update_image_region(img, region, data, advance_active_slot);
#endif
}

// Internal function to process all queued updates for a single image
_SOKOL_PRIVATE void _sgext_process_image_update_queue(_sg_image_t* img, std::list<_sgext_image_update_info>& enqueued_updates)
{
    // The image was already partially updated this frame, skip it
    // This can happen when there are enqueued updates and the image is re-used
    // multiple times in the frame - we only want the first update
    if (img->cmn.upd_frame_index == _sg.frame_index)
        return;

    for (auto it = enqueued_updates.begin(); it != enqueued_updates.end(); /* no increment here */)
    {
        _sgext_update_image_region_internal(img, &it->region, &it->data, true);

        img->cmn.upd_frame_index = _sg.frame_index;
        it->update_frame_count++;

        // Keep the update around for SG_NUM_INFLIGHT_FRAMES to ensure all inflight frames see it
        if (it->update_frame_count >= SG_NUM_INFLIGHT_FRAMES)
        {
            _sgext_free_image_update_info(*it);
            it = enqueued_updates.erase(it);
        }
        else
            ++it;
    }
}

// ██████  ██    ██ ██████  ██      ██  ██████
// ██   ██ ██    ██ ██   ██ ██      ██ ██
// ██████  ██    ██ ██████  ██      ██ ██
// ██      ██    ██ ██   ██ ██      ██ ██
// ██       ██████  ██████  ███████ ██  ██████
//
// >>public

void sgext_update_image_region(sg_image img_id, const sgext_image_region* region, const sg_image_data* data, bool advance_active_slot)
{
    _sg_image_t* img = _sg_lookup_image(img_id.id);
    _sgext_update_image_region_internal(img, region, data, advance_active_slot);
}

void sgext_apply_image_updates(const sg_bindings* bindings)
{
    for (int i = 0; i < SG_MAX_VIEW_BINDSLOTS; i++)
    {
        sg_view view = bindings->views[i];
        if (view.id == SG_INVALID_ID)
            continue;

        // Query the underlying image from the view
        sg_image img = sg_query_view_image(view);
        if (img.id == SG_INVALID_ID)
            continue;

        auto it = g_image_update_queue.find(img.id);
        if (it != g_image_update_queue.end())
        {
            _sg_image_t* img_ptr = _sg_lookup_image(img.id);
            _sgext_process_image_update_queue(img_ptr, it->second);
        }
    }
}

void sgext_enable_image_queue(sg_image img, bool enabled)
{
    auto it = g_image_update_queue.find(img.id);
    if (enabled)
    {
        if (it == g_image_update_queue.end())
            g_image_update_queue[img.id] = std::list<_sgext_image_update_info>();
    }
    else
    {
        if (it != g_image_update_queue.end())
        {
            for (auto& entry : it->second)
                _sgext_free_image_update_info(entry);
            g_image_update_queue.erase(it);
        }
    }
}

void sgext_queue_image_update(sg_image img, const sgext_image_region* region, const sg_image_data* data, sgext_ownership ownership)
{
    // If image only has 1 slot, there's no need to enqueue updates, do it immediately
    auto info = sg_query_image_info(img);
    if (info.num_slots == 1)
    {
        _sg_image_t* img_ptr = _sg_lookup_image(img.id);
        _sgext_update_image_region_internal(img_ptr, region, data, false);
        return;
    }

    SOKOL_ASSERT(ownership < _SGEXT_OWNERSHIP_NUM);
    if (ownership == _SGEXT_OWNERSHIP_DEFAULT)
        ownership = SGEXT_OWNERSHIP_REFERENCE;

    auto it = g_image_update_queue.find(img.id);
    SOKOL_ASSERT(it != g_image_update_queue.end());

    _sgext_image_update_info entry {
        .region = *region,
        .update_frame_count = 0,
        .ownership = ownership
    };

    _sgext_copy_image_data_or_fill(*data, entry.data, ownership == SGEXT_OWNERSHIP_COPY);

    it->second.push_back(entry);
}

#endif // SOKOL_GFX_EXT_PARTIAL_UPDATE_IMPL
