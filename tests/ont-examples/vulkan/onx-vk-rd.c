#include <onex-kernel/log.h>

#include "user-onl-vk.h"

static VkBuffer vertex_buffer;
static VkBuffer staging_buffer;

static VkDeviceMemory vertex_buffer_memory;
static VkDeviceMemory staging_buffer_memory;

struct uniforms {
    float proj[4][4];
    float view_l[4][4];
    float view_r[4][4];
    float model[MAX_PANELS][4][4];
};

typedef struct {
    VkBuffer        uniform_buffer;
    VkDeviceMemory  uniform_memory;
    void*           uniform_memory_ptr;
    VkDescriptorSet descriptor_set;
} uniform_mem_t;

static uniform_mem_t *uniform_mem;

static VkDescriptorPool descriptor_pool;

static VkFormatProperties format_properties;

static void prepare_vertex_buffers(){

  VkBufferCreateInfo buffer_ci = {
    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
    .size = MAX_PANELS * 6*6 * (3 * sizeof(float) +
                                2 * sizeof(float)  ),
    .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
    .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
  };

  create_buffer_with_memory(&buffer_ci,
                             VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                             VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                            &vertex_buffer,
                            &vertex_buffer_memory);
}

void onx_vk_rd_prepare_uniform_buffers(bool restart) {

  prepare_vertex_buffers();

  uniform_mem = (uniform_mem_t*)malloc(sizeof(uniform_mem_t) * max_img);

  VkBufferCreateInfo buffer_ci = {
     .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
     .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
     .size = sizeof(struct uniforms),
  };
  for (uint32_t ii = 0; ii < max_img; ii++) {

    create_buffer_with_memory(&buffer_ci,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              &uniform_mem[ii].uniform_buffer,
                              &uniform_mem[ii].uniform_memory);

    VK_CHECK(vkMapMemory(device,
                         uniform_mem[ii].uniform_memory,
                         0, sizeof(struct uniforms), 0,
                        &uniform_mem[ii].uniform_memory_ptr));
  }
}

static void do_cmd_buf_draw(uint32_t ii, VkCommandBuffer cmd_buf){

  VkBuffer vertex_buffers[] = { vertex_buffer, };
  VkDeviceSize offsets[] = { 0 };
  vkCmdBindVertexBuffers(cmd_buf, 0, 1, vertex_buffers, offsets);

  vkCmdBindDescriptorSets(cmd_buf,
                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline_layout,
                          0, 1,
                          &uniform_mem[ii].descriptor_set,
                          0, NULL);

  struct push_constants pc;

  pc.phase = 0, // ground plane
  vkCmdPushConstants(cmd_buf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(struct push_constants), &pc);
  vkCmdDraw(cmd_buf, 6, 1, 0, 0);

  pc.phase = 1, // panels
  vkCmdPushConstants(cmd_buf, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                     sizeof(struct push_constants), &pc);
  vkCmdDraw(cmd_buf, 6*6, 1, 0, 0);
}

void set_up_scene_begin(float** vertices) {

  pthread_mutex_lock(&scene_lock);
  scene_ready = false;

  size_t vertex_size = MAX_PANELS * 6*6 * (3 * sizeof(float) +
                                           2 * sizeof(float)  );

  VK_CHECK(vkMapMemory(device, vertex_buffer_memory, 0, vertex_size, 0, (void**)vertices));
}

void set_up_scene_end() {

  vkUnmapMemory(device, vertex_buffer_memory);

  for (uint32_t ii = 0; ii < max_img; ii++) {
      VkCommandBuffer cmd_buf = begin_cmd_buf(ii);
      begin_render_pass(ii, cmd_buf);
      do_cmd_buf_draw(ii, cmd_buf);
      end_cmd_buf_and_render_pass(ii, cmd_buf);
  }

  scene_ready = true;
  pthread_mutex_unlock(&scene_lock);
}

void onx_vk_rd_update_uniforms() {

  set_proj_view();

  memcpy(uniform_mem[cur_img].uniform_memory_ptr,
         (const void*)&proj_matrix,    sizeof(proj_matrix));

  memcpy(uniform_mem[cur_img].uniform_memory_ptr +
                sizeof(proj_matrix),
         (const void*)&view_l_matrix,  sizeof(view_l_matrix));

  memcpy(uniform_mem[cur_img].uniform_memory_ptr +
                sizeof(proj_matrix)+sizeof(view_l_matrix),
         (const void*)&view_r_matrix,  sizeof(view_r_matrix));

  memcpy(uniform_mem[cur_img].uniform_memory_ptr +
                sizeof(proj_matrix)+sizeof(view_l_matrix)+sizeof(view_r_matrix),
         (const void*)&model_matrix, sizeof(model_matrix));
}

struct texture_object {
    int32_t texture_width;
    int32_t texture_height;
    VkSampler sampler;
    VkBuffer buffer;
    VkImageLayout image_layout;
    VkDeviceMemory device_memory;
    VkImage image;
    VkImageView image_view;
};

static char* texture_files[] = { "ivory.ppm" }; // not used

#include "ivory.ppm.h"  // is used

#define TEXTURE_COUNT 1

struct texture_object textures[TEXTURE_COUNT];
struct texture_object staging_texture;

static VkFormat texture_format = VK_FORMAT_R8G8B8A8_UNORM;

static bool load_texture(const char *filename,
                         uint8_t *rgba_data,
                         uint64_t row_pitch,
                         int32_t *w, int32_t *h) {
    (void)filename;
    char *cPtr;
    cPtr = (char *)texture_array;
    if((unsigned char*)cPtr >= (texture_array + texture_len) || strncmp(cPtr, "P6\n", 3)) {
        return false;
    }
    while (strncmp(cPtr++, "\n", 1)) ;
    sscanf(cPtr, "%u %u", w, h);

    if(!rgba_data) return true;

    while (strncmp(cPtr++, "\n", 1)) { }

    if((unsigned char*)cPtr >= (texture_array + texture_len) || strncmp(cPtr, "255\n", 4)) {
        return false;
    }
    while (strncmp(cPtr++, "\n", 1)) { }

    for (int y = 0; y < *h; y++) {
        uint8_t *rowPtr = rgba_data;
        for (int x = 0; x < *w; x++) {
            memcpy(rowPtr, cPtr, 3);
            rowPtr[3] = 255; /* Alpha of 1 */
            rowPtr += 4;
            cPtr += 3;
        }
        rgba_data += row_pitch;
    }
    return true;
}

// ---------------------------------

static void prepare_texture_image(const char *filename,
                                  struct texture_object *texture_obj,
                                  VkImageTiling tiling,
                                  VkImageUsageFlags usage,
                                  VkFlags prop_flags) {
    int32_t texture_width;
    int32_t texture_height;
    VkResult err;

    if (!load_texture(filename, NULL, 0, &texture_width, &texture_height)) {
        ERR_EXIT("Failed to load textures");
    }

    texture_obj->texture_width = texture_width;
    texture_obj->texture_height = texture_height;

    VkImageCreateInfo image_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = texture_format,
        .extent = {texture_width, texture_height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = tiling,
        .usage = usage,
        .flags = 0,
        .initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED,
    };

    uint32_t size = create_image_with_memory(&image_ci,
                                             prop_flags,
                                             &texture_obj->image,
                                             &texture_obj->device_memory);

    if (prop_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
        const VkImageSubresource subres = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = 0,
            .arrayLayer = 0,
        };
        VkSubresourceLayout layout;

        vkGetImageSubresourceLayout(device, texture_obj->image, &subres, &layout);

        void *data;
        VK_CHECK(vkMapMemory(device, texture_obj->device_memory, 0, size, 0, &data));

        if(!load_texture(filename, data, layout.rowPitch, &texture_width, &texture_height)){
            log_write("Error loading texture: %s\n", filename);
        }
        vkUnmapMemory(device, texture_obj->device_memory);
    }
    texture_obj->image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

static void prepare_texture_buffer(char *filename, struct texture_object *texture_obj) {

    int32_t texture_width;
    int32_t texture_height;
    VkResult err;

    if (!load_texture(filename, 0, 0, &texture_width, &texture_height)) {
        ERR_EXIT("Failed to load textures");
    }

    texture_obj->texture_width = texture_width;
    texture_obj->texture_height = texture_height;

    VkBufferCreateInfo buffer_ci = {
      .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
      .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      .flags = 0,
      .size = texture_width * texture_height * 4,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
      .pNext = 0,
    };

    VkFlags prop_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    uint32_t size = create_buffer_with_memory(&buffer_ci,
                                              prop_flags,
                                              &texture_obj->buffer,
                                              &texture_obj->device_memory);
    void *data;
    err = vkMapMemory(device, texture_obj->device_memory, 0, size, 0, &data);
    assert(!err);

    VkSubresourceLayout layout;
    memset(&layout, 0, sizeof(layout));
    layout.rowPitch = texture_width * 4;

    if (!load_texture(filename, data, layout.rowPitch, &texture_width, &texture_height)) {
        log_write("Error loading texture: %s\n", filename);
    }

    vkUnmapMemory(device, texture_obj->device_memory);
}

static void prepare_textures(){

    uint32_t i;

    for (i = 0; i < TEXTURE_COUNT; i++) {
        VkResult err;

#if defined(LIMIT_TO_LINEAR_AND_NO_STAGING_BUFFER)
        if((format_properties.linearTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT)){

            prepare_texture_image(texture_files[i], &textures[i],
                                  VK_IMAGE_TILING_LINEAR,
                                  VK_IMAGE_USAGE_SAMPLED_BIT,
                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            transition_image(initcmd, textures[i].image,
                               VK_IMAGE_LAYOUT_PREINITIALIZED,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               0,
                               VK_ACCESS_SHADER_READ_BIT |
                               VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);

            staging_texture.image = 0;

        } else
#endif
        if(format_properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT){

            memset(&staging_texture, 0, sizeof(staging_texture));
            prepare_texture_buffer(texture_files[i], &staging_texture);

            prepare_texture_image(texture_files[i], &textures[i],
                                  VK_IMAGE_TILING_OPTIMAL,
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            transition_image(initcmd, textures[i].image,
                               VK_IMAGE_LAYOUT_PREINITIALIZED,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               0, VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT);

            VkBufferImageCopy copy_region = {
                .bufferOffset = 0,
                .bufferRowLength = staging_texture.texture_width,
                .bufferImageHeight = staging_texture.texture_height,
                .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
                .imageOffset = {0, 0, 0},
                .imageExtent = { staging_texture.texture_width,
                                 staging_texture.texture_height, 1 },
            };

            vkCmdCopyBufferToImage(initcmd, staging_texture.buffer, textures[i].image,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

            transition_image(initcmd, textures[i].image,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                               VK_ACCESS_TRANSFER_WRITE_BIT,
                               VK_ACCESS_SHADER_READ_BIT |
                               VK_ACCESS_INPUT_ATTACHMENT_READ_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT,
                               VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
        } else {
            assert(!"No support for R8G8B8A8_UNORM as texture image format");
        }

        VkSamplerCreateInfo sampler_ci = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .pNext = NULL,
            .magFilter = VK_FILTER_NEAREST,
            .minFilter = VK_FILTER_NEAREST,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST,
            .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
            .mipLodBias = 0.0f,
            .anisotropyEnable = VK_FALSE,
            .maxAnisotropy = 1,
            .compareOp = VK_COMPARE_OP_NEVER,
            .minLod = 0.0f,
            .maxLod = 0.0f,
            .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
            .unnormalizedCoordinates = VK_FALSE,
        };

        VkImageViewCreateInfo image_view_ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = NULL,
            .image = VK_NULL_HANDLE,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texture_format,
            .components = {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange = {
                .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel   = 0,
                .levelCount     = 1,
                .baseArrayLayer = 0,
                .layerCount     = 1,
            },
            .flags = 0,
        };

        err = vkCreateSampler(device, &sampler_ci, NULL, &textures[i].sampler);
        assert(!err);

        image_view_ci.image = textures[i].image;
        VK_CHECK(vkCreateImageView(device, &image_view_ci, NULL, &textures[i].image_view));
    }
}

// ---------------------------------

VkVertexInputBindingDescription vibds[] = {
  {
    .binding = 0,
    .stride = 3 * sizeof(float) +
              2 * sizeof(float),
    .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  }
};

VkVertexInputAttributeDescription vertex_input_attributes[] = {
  { 0, 0, VK_FORMAT_R32G32B32_SFLOAT,    0 }, // vertex
  { 1, 0, VK_FORMAT_R32G32_SFLOAT,      12 }, // uv
};

void onx_vk_rd_prepare_render_data(bool restart) {

  vkGetPhysicalDeviceFormatProperties(gpu, texture_format, &format_properties);

  prepare_textures();

  // ----------

  vertex_input_state_ci.vertexBindingDescriptionCount = 1;
  vertex_input_state_ci.pVertexBindingDescriptions = vibds;
  vertex_input_state_ci.vertexAttributeDescriptionCount = 2;
  vertex_input_state_ci.pVertexAttributeDescriptions = vertex_input_attributes;
}

// ----------------------------------------------------------------------------------------

void onx_vk_rd_prepare_descriptor_pool(bool restart) {

  VkDescriptorPoolSize pool_sizes[] = {
      { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        max_img                 },
      { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        max_img * 3             },
      { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        max_img * TEXTURE_COUNT },
  };

  VkDescriptorPoolCreateInfo descriptor_pool_ci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = (max_img * 3) + max_img + (max_img * TEXTURE_COUNT) + 23, //// ??
      .poolSizeCount = 3,
      .pPoolSizes = pool_sizes,
      .pNext = 0,
  };

  VK_CHECK(vkCreateDescriptorPool(device, &descriptor_pool_ci, NULL, &descriptor_pool));
}

void onx_vk_rd_prepare_descriptor_set(bool restart) {

  VkDescriptorSetAllocateInfo descriptor_set_ai = {
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = descriptor_pool,
    .descriptorSetCount = 1,
    .pSetLayouts = &descriptor_layout,
    .pNext = 0,
  };

  VkDescriptorBufferInfo uniform_info = {
    .offset = 0,
    .range = sizeof(struct uniforms),
  };

  VkDescriptorImageInfo texture_descs[TEXTURE_COUNT];
  memset(&texture_descs, 0, sizeof(texture_descs));

  for (unsigned int i = 0; i < TEXTURE_COUNT; i++) {
    texture_descs[i].sampler = textures[i].sampler;
    texture_descs[i].imageView = textures[i].image_view;
    texture_descs[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  }

  VkWriteDescriptorSet writes[] = {
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
      .dstBinding = 0,
      .pBufferInfo = &uniform_info,
      .descriptorCount = 1,
    },
    {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .dstBinding = 1,
      .pImageInfo = texture_descs,
      .descriptorCount = TEXTURE_COUNT,
    },
  };

  for (uint32_t i = 0; i < max_img; i++) {

    VK_CHECK(vkAllocateDescriptorSets(
                              device,
                             &descriptor_set_ai,
                             &uniform_mem[i].descriptor_set));

    uniform_info.buffer = uniform_mem[i].uniform_buffer;
    writes[0].dstSet = uniform_mem[i].descriptor_set;
    writes[1].dstSet = uniform_mem[i].descriptor_set;

    vkUpdateDescriptorSets(device, 2, writes, 0, 0);
  }
}


void onx_vk_rd_prepare_descriptor_layout(bool restart) {

  VkDescriptorSetLayoutBinding bindings[] = {
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .pImmutableSamplers = NULL,
      },
      {
          .binding = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = TEXTURE_COUNT,
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .pImmutableSamplers = NULL,
      },
  };

  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = 2,
      .pBindings = bindings,
  };

  VK_CHECK(vkCreateDescriptorSetLayout(device,
                                       &descriptor_set_layout_ci,
                                       0,
                                       &descriptor_layout));
}

extern unsigned char tests_ont_examples_vulkan_onx_frag_spv[];
extern unsigned int  tests_ont_examples_vulkan_onx_frag_spv_len;
extern unsigned char tests_ont_examples_vulkan_onx_vert_spv[];
extern unsigned int  tests_ont_examples_vulkan_onx_vert_spv_len;

VkShaderModule vert_shader_module;
VkShaderModule frag_shader_module;

static VkShaderModule load_c_shader(bool load_frag) {

    VkShaderModuleCreateInfo module_ci = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = load_frag? tests_ont_examples_vulkan_onx_frag_spv_len:
                               tests_ont_examples_vulkan_onx_vert_spv_len,
        .pCode = load_frag? tests_ont_examples_vulkan_onx_frag_spv:
                            tests_ont_examples_vulkan_onx_vert_spv,
        .flags = 0,
        .pNext = 0,
    };

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device,
                                  &module_ci,
                                  0,
                                  &module));
    return module;
}

void onx_vk_rd_prepare_shaders(bool restart){
  vert_shader_module = load_c_shader(false);
  frag_shader_module = load_c_shader(true);
}

// ---------------------------------

void onx_vk_rd_finish_render_data() {

  scene_ready = false;

  // ---------------------------------

  vkFreeMemory(device, vertex_buffer_memory, NULL);
  vkFreeMemory(device, staging_buffer_memory, NULL);

  vkDestroyBuffer(device, vertex_buffer, NULL);
  vkDestroyBuffer(device, staging_buffer, NULL);

  // ---------------------------------

  vkDestroyDescriptorPool(device, descriptor_pool, NULL);
  vkDestroyDescriptorSetLayout(device, descriptor_layout, NULL);

  // ---------------------------------

  for (uint32_t i = 0; i < max_img; i++) {
      vkDestroyBuffer(device, uniform_mem[i].uniform_buffer, NULL);
      vkUnmapMemory(device, uniform_mem[i].uniform_memory);
      vkFreeMemory(device, uniform_mem[i].uniform_memory, NULL);
  }
  free(uniform_mem);

  // ---------------------------------

  for (uint32_t i = 0; i < TEXTURE_COUNT; i++) {
    vkDestroyImageView(device, textures[i].image_view, NULL);
    vkDestroyImage(device, textures[i].image, NULL);
    vkFreeMemory(device, textures[i].device_memory, NULL);
    vkDestroySampler(device, textures[i].sampler, NULL);
  }
  if(staging_texture.buffer) {
     vkFreeMemory(device, staging_texture.device_memory, NULL);
     if(staging_texture.image) vkDestroyImage(device, staging_texture.image, NULL);
     vkDestroyBuffer(device, staging_texture.buffer, NULL);
  }

  // ---------------------------------
}



