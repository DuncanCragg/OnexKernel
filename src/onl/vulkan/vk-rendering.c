// ---------------------------------------------------------------------------------------

#include <onex-kernel/log.h>

#include <onl-vk.h>
#include "onl/vulkan/vk.h"

#define VK_DESTROY(func, dev, obj) func(dev, obj, NULL), obj = NULL

uint32_t onl_vk_width;
uint32_t onl_vk_height;

float onl_vk_aspect_ratio; // REVISIT: float?

VkShaderModule onl_vk_vert_shader_module;
VkShaderModule onl_vk_frag_shader_module;

#define ONE_EYE  1
#define TWO_EYES 2

uint32_t onl_vk_max_img;
uint32_t onl_vk_cur_img;

VkPipelineVertexInputStateCreateInfo onl_vk_vertex_input_state_ci = {
  .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
};

uint8_t         onl_vk_render_stage = ONL_VK_RENDER_STAGE_PREPARE;
pthread_mutex_t onl_vk_render_stage_lock;

VkFormat            onl_vk_texture_format = VK_FORMAT_R8G8B8A8_UNORM;
VkFormatProperties  onl_vk_texture_format_properties;

// --------------

static bool sbs_render = false;

static VkRenderPass render_pass;

static VkSemaphore image_acquired_semaphore;
static VkSemaphore render_complete_semaphore;

VkPipelineLayout onl_vk_pipeline_layout;

static VkPipeline       pipeline;
static VkPipelineCache  pipeline_cache;

static VkPhysicalDeviceMemoryProperties memory_properties;

// ---------------------------------

struct {
    VkFormat format;
    VkDeviceMemory device_memory;
    VkImage image;
    VkImageView image_view;
} color;

struct {
    VkFormat format;
    VkDeviceMemory device_memory;
    VkImage image;
    VkImageView image_view;
} depth;

typedef struct {
    VkFramebuffer   framebuffer;
    VkImage         image;
    VkImageView     image_view;
    VkCommandBuffer cmd_buf;
    VkFence         cmd_buf_fence;
} SwapchainBits;

static SwapchainBits* swapchain_bits;

void onl_vk_transition_image(VkCommandBuffer cmdBuffer,
                             VkImage image,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout,
                             VkAccessFlagBits srcAccessMask,
                             VkAccessFlagBits dstAccessMask,
                             VkPipelineStageFlags srcStage,
                             VkPipelineStageFlags dstStage) {

    VkImageMemoryBarrier img_mem_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .image = image,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .subresourceRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount     = VK_REMAINING_ARRAY_LAYERS,
        },
        .pNext = NULL,
    };

    vkCmdPipelineBarrier(
        cmdBuffer,
        srcStage,
        dstStage,
        0,
        0, NULL,
        0, NULL,
        1,
        &img_mem_barrier
    );
}

void copy_colour_to_swap(uint32_t ii) {

  VkCommandBuffer cmd_buf = swapchain_bits[ii].cmd_buf;

  onl_vk_transition_image(
      cmd_buf,
      color.image,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT
  );

  onl_vk_transition_image(
      cmd_buf,
      swapchain_bits[ii].image,
      VK_IMAGE_LAYOUT_UNDEFINED,  // known layout???
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      0,
      VK_ACCESS_TRANSFER_READ_BIT,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT
  );

  VkImageCopy copy_spec = {
    .srcSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .dstSubresource = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .mipLevel = 0,
      .baseArrayLayer = 0,
      .layerCount = 1,
    },
    .srcOffset = {0, 0, 0},
    .dstOffset = {0, 0, 0},
    .extent = {
      .width = swapchain_extent.width / 2,
      .height = swapchain_extent.height,
      .depth = 1,
    }
  };

  vkCmdCopyImage(
      cmd_buf,
      color.image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      swapchain_bits[ii].image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &copy_spec
  );

  copy_spec.srcSubresource.baseArrayLayer = 1;
  copy_spec.dstOffset.x = swapchain_extent.width / 2,

  vkCmdCopyImage(
      cmd_buf,
      color.image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      swapchain_bits[ii].image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      1,
      &copy_spec
  );

  onl_vk_transition_image(
      cmd_buf,
      swapchain_bits[ii].image,
      VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      VK_ACCESS_TRANSFER_WRITE_BIT,
      VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  );

  onl_vk_transition_image(
      cmd_buf,
      color.image,
      VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      0,
      VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
      VK_PIPELINE_STAGE_TRANSFER_BIT,
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  );
}

void onl_vk_render_frame() {

  vkWaitForFences(onl_vk_device, 1, &swapchain_bits[onl_vk_cur_img].cmd_buf_fence, VK_TRUE, UINT64_MAX);

  pthread_mutex_lock(&onl_vk_render_stage_lock);
  if(onl_vk_render_stage != ONL_VK_RENDER_STAGE_RUNNING){
    pthread_mutex_unlock(&onl_vk_render_stage_lock);
    return;
  }

  VkResult err;
  do {
      err = vkAcquireNextImageKHR(onl_vk_device,
                                  swapchain,
                                  UINT64_MAX,
                                  image_acquired_semaphore,
                                  VK_NULL_HANDLE,
                                  &onl_vk_cur_img);

      if (err == VK_SUCCESS || err == VK_SUBOPTIMAL_KHR){
        break;
      }
      else
      if (err == VK_ERROR_OUT_OF_DATE_KHR) {
        pthread_mutex_unlock(&onl_vk_render_stage_lock); // ??
        onl_vk_restart();
      }
      else {
        pthread_mutex_unlock(&onl_vk_render_stage_lock);
        return;
      }
  } while(true);

  VkPipelineStageFlags wait_stages[] = {
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
  };
  VkSubmitInfo submit_info = {
    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
    .pWaitDstStageMask = wait_stages,
    .waitSemaphoreCount = 1,
    .signalSemaphoreCount = 1,
    .commandBufferCount = 1,
  };

  VkSemaphore img_acq_semaphore[] = { image_acquired_semaphore };
  VkSemaphore ren_com_semaphore[] = { render_complete_semaphore };

  submit_info.pWaitSemaphores   = img_acq_semaphore;
  submit_info.pSignalSemaphores = ren_com_semaphore;
  submit_info.pCommandBuffers = &swapchain_bits[onl_vk_cur_img].cmd_buf,

  vkResetFences(onl_vk_device, 1, &swapchain_bits[onl_vk_cur_img].cmd_buf_fence);

  err = vkQueueSubmit(queue, 1, &submit_info,
                      swapchain_bits[onl_vk_cur_img].cmd_buf_fence);

  VkPresentInfoKHR present_info = {
    .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
    .waitSemaphoreCount = 1,
    .pWaitSemaphores = ren_com_semaphore,
    .swapchainCount = 1,
    .pSwapchains = &swapchain,
    .pImageIndices = &onl_vk_cur_img,
    .pNext = NULL,
  };

  err = vkQueuePresentKHR(queue, &present_info);

  pthread_mutex_unlock(&onl_vk_render_stage_lock); // ??
  if (err == VK_ERROR_OUT_OF_DATE_KHR || err == VK_SUBOPTIMAL_KHR) {
    onl_vk_restart();
  }
}

// ---------------------------------

static bool memory_type_from_properties(uint32_t typeBits,
                                        VkFlags requirements_mask,
                                        uint32_t *typeIndex) {

    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if ((typeBits & 1) == 1) {
            if ((memory_properties.memoryTypes[i].propertyFlags &
                 requirements_mask) == requirements_mask) {

                *typeIndex = i;
                return true;
            }
        }
        typeBits >>= 1;
    }
    return false;
}

uint32_t onl_vk_create_buffer_with_memory(VkBufferCreateInfo*   buffer_ci,
                                          VkMemoryPropertyFlags prop_flags,
                                          VkBuffer*             buffer,
                                          VkDeviceMemory*       memory){

  ONL_VK_CHECK_EXIT(vkCreateBuffer(onl_vk_device, buffer_ci, 0, buffer));

  VkMemoryRequirements mem_reqs;
  vkGetBufferMemoryRequirements(onl_vk_device, *buffer, &mem_reqs);

  VkMemoryAllocateInfo memory_ai = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = mem_reqs.size,
  };
  assert(memory_type_from_properties(mem_reqs.memoryTypeBits,
                                     prop_flags,
                                     &memory_ai.memoryTypeIndex));

  ONL_VK_CHECK_EXIT(vkAllocateMemory(onl_vk_device, &memory_ai, 0, memory));

  ONL_VK_CHECK_EXIT(vkBindBufferMemory(onl_vk_device, *buffer, *memory, 0));

  return memory_ai.allocationSize;
}

uint32_t onl_vk_create_image_with_memory(VkImageCreateInfo*    image_ci,
                                         VkMemoryPropertyFlags prop_flags,
                                         VkImage*              image,
                                         VkDeviceMemory*       memory) {

  ONL_VK_CHECK_EXIT(vkCreateImage(onl_vk_device, image_ci, 0, image));

  VkMemoryRequirements mem_reqs;
  vkGetImageMemoryRequirements(onl_vk_device, *image, &mem_reqs);

  VkMemoryAllocateInfo memory_ai = {
    .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
    .allocationSize = mem_reqs.size,
  };
  assert(memory_type_from_properties(mem_reqs.memoryTypeBits,
                                     prop_flags,
                                     &memory_ai.memoryTypeIndex));

  ONL_VK_CHECK_EXIT(vkAllocateMemory(onl_vk_device, &memory_ai, 0, memory));

  ONL_VK_CHECK_EXIT(vkBindImageMemory(onl_vk_device, *image, *memory, 0));

  return memory_ai.allocationSize;
}

static void prepare_color() {

  VkImageCreateInfo image_ci = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
    .imageType = VK_IMAGE_TYPE_2D,
    .format = surface_format,
    .extent = { swap_width, swap_height, 1 },
    .mipLevels = 1,
    .arrayLayers = TWO_EYES,
    .samples = VK_SAMPLE_COUNT_1_BIT,
    .tiling = VK_IMAGE_TILING_OPTIMAL,
    .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
             VK_IMAGE_USAGE_SAMPLED_BIT |
             VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
  };

  color.format = surface_format;

  onl_vk_create_image_with_memory(&image_ci,
                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                  &color.image,
                                  &color.device_memory);

  VkImageViewCreateInfo image_view_ci = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = NULL,
      .image = color.image,
      .format = surface_format,
      .subresourceRange = {
         .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
         .baseMipLevel   = 0,
         .levelCount     = 1,
         .baseArrayLayer = 0,
         .layerCount     = TWO_EYES,
      },
      .flags = 0,
      .viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY,
  };

  ONL_VK_CHECK_EXIT(vkCreateImageView(onl_vk_device, &image_view_ci,
                                      0, &color.image_view));
}

static void prepare_depth() {

    const VkFormat depth_format = VK_FORMAT_D16_UNORM;

    VkImageCreateInfo image_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = NULL,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depth_format,
        .extent = { swap_width, swap_height, 1 },
        .mipLevels = 1,
        .arrayLayers = sbs_render? TWO_EYES: ONE_EYE,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .flags = 0,
    };

    depth.format = depth_format;

    VkMemoryPropertyFlags prop_flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    onl_vk_create_image_with_memory(&image_ci,
                                    prop_flags,
                                    &depth.image,
                                    &depth.device_memory);

    VkImageViewCreateInfo image_view_ci = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = NULL,
        .image = depth.image,
        .format = depth_format,
        .subresourceRange = {
           .aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT,
           .baseMipLevel   = 0,
           .levelCount     = 1,
           .baseArrayLayer = 0,
           .layerCount     = sbs_render? TWO_EYES: ONE_EYE,
        },
        .flags = 0,
        .viewType = sbs_render? VK_IMAGE_VIEW_TYPE_2D_ARRAY:
                                VK_IMAGE_VIEW_TYPE_2D,
    };

    ONL_VK_CHECK_EXIT(vkCreateImageView(onl_vk_device, &image_view_ci,
                                        0, &depth.image_view));
}

// -------------------------------------------------------------------------------------

void onl_vk_prepare_swapchain_images(bool restart) {

    float aspect_ratio = (float)swap_width / (float)swap_height;
    sbs_render = aspect_ratio > 2.0f;
    onl_vk_aspect_ratio = aspect_ratio / (sbs_render? 2.0f: 1.0f);

    onl_vk_width  = swap_width / (sbs_render? 2: 1);
    onl_vk_height = swap_height;

    log_write("aspect_ratio %.3f/%.3f SBS=%s\n",
                                   aspect_ratio,
                                   onl_vk_aspect_ratio,
                                   sbs_render? "ON": "OFF");

    VkResult err;
    err = vkGetSwapchainImagesKHR(onl_vk_device, swapchain, &onl_vk_max_img, NULL);
    assert(!err);

    VkImage *swapchainImages = (VkImage *)malloc(onl_vk_max_img * sizeof(VkImage));
    assert(swapchainImages);
    err = vkGetSwapchainImagesKHR(onl_vk_device, swapchain, &onl_vk_max_img, swapchainImages);
    assert(!err);

    swapchain_bits = (SwapchainBits*)malloc(sizeof(SwapchainBits) * onl_vk_max_img);

    for (uint32_t i = 0; i < onl_vk_max_img; i++) {
        VkImageViewCreateInfo image_view_ci = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = swapchainImages[i],
            .pNext = NULL,
            .format = surface_format,
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
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .flags = 0,
        };

        swapchain_bits[i].image = swapchainImages[i];
        ONL_VK_CHECK_EXIT(vkCreateImageView(onl_vk_device, &image_view_ci, NULL,
                                            &swapchain_bits[i].image_view));
    }

    if (NULL != swapchainImages) {
        free(swapchainImages);
    }
}

void onl_vk_prepare_semaphores_and_fences(bool restart) {

  VkFenceCreateInfo fence_ci = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
      .pNext = 0,
  };

  for (uint32_t i = 0; i < onl_vk_max_img; i++) {
      ONL_VK_CHECK_EXIT(vkCreateFence(onl_vk_device, &fence_ci,
                                      0, &swapchain_bits[i].cmd_buf_fence));
  }

  VkSemaphoreCreateInfo semaphore_ci = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = 0,
  };

  ONL_VK_CHECK_EXIT(vkCreateSemaphore(onl_vk_device, &semaphore_ci,
                                      0, &image_acquired_semaphore));
  ONL_VK_CHECK_EXIT(vkCreateSemaphore(onl_vk_device, &semaphore_ci,
                                      0, &render_complete_semaphore));
}

void onl_vk_prepare_command_buffers(bool restart){

  VkCommandBufferAllocateInfo cmd_buf_ai = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
      .pNext = 0,
  };

  for (uint32_t i = 0; i < onl_vk_max_img; i++) {
      ONL_VK_CHECK_EXIT(vkAllocateCommandBuffers(
                       onl_vk_device,
                       &cmd_buf_ai,
                       &swapchain_bits[i].cmd_buf
      ));
  }
}

void onl_vk_prepare_rendering(bool restart) {
  vkGetPhysicalDeviceMemoryProperties(gpu, &memory_properties);
  vkGetPhysicalDeviceFormatProperties(gpu, onl_vk_texture_format,
                                           &onl_vk_texture_format_properties);
  if(sbs_render) prepare_color();
  prepare_depth();
}

void onl_vk_prepare_render_pass(bool restart) {
    const VkAttachmentDescription attachments[2] = {
            {
                .format = sbs_render? color.format: surface_format,
                .flags = 0,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = sbs_render? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
                                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            {
                .format = depth.format,
                .flags = 0,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
                .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            },
    };
    const VkAttachmentReference color_reference = {
        .attachment = 0, // this refers to the color attachment
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };
    const VkAttachmentReference depth_reference = {
        .attachment = 1, // this refers to the depth attachment
        .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    };
    const VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .flags = 0,
        .inputAttachmentCount = 0,
        .pInputAttachments = NULL,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_reference,
        .pResolveAttachments = NULL,
        .pDepthStencilAttachment = &depth_reference,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = NULL,
    };

    VkSubpassDependency attachmentDependencies[2] = {
            {
                .srcSubpass = sbs_render? 0: VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                .srcAccessMask = sbs_render? VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT: 0,
                .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                                 VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
                .dependencyFlags = sbs_render? VK_DEPENDENCY_BY_REGION_BIT |
                                               VK_DEPENDENCY_VIEW_LOCAL_BIT: 0,
            },
            {
                .srcSubpass = VK_SUBPASS_EXTERNAL,
                .dstSubpass = 0,
                .srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                .dependencyFlags = 0,
            },
    };

    VkRenderPassCreateInfo rp_ci = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .attachmentCount = 2,
        .pAttachments = attachments,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 2,
        .pDependencies = attachmentDependencies,
    };

    const uint32_t viewMask        = 0b00000011;
    const uint32_t correlationMask = 0b00000011;

    VkRenderPassMultiviewCreateInfo rpmv_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
        .subpassCount = 1,
        .pViewMasks = &viewMask,
        .correlationMaskCount = 1,
        .pCorrelationMasks = &correlationMask,
    };

    if(sbs_render){
      rp_ci.pNext = &rpmv_info;
    }
    VkResult err;
    err = vkCreateRenderPass(onl_vk_device, &rp_ci, NULL, &render_pass);
    assert(!err);
}

void onl_vk_prepare_pipeline(bool restart) {

  VkPipelineShaderStageCreateInfo shader_stages_ci[] = {
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = onl_vk_vert_shader_module,
      .pName = "main",
    },
    {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = onl_vk_frag_shader_module,
      .pName = "main",
    }
  };

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE,
  };

  VkViewport viewport = {
      .x = 0.0f,
      .y = 0.0f,
      .width  = swap_width / (sbs_render? 2: 1),
      .height = swap_height,
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  VkRect2D scissor = {
      .offset = {
         0,
         0
      },
      .extent = {
         swap_width / (sbs_render? 2: 1),
         swap_height,
      },
  };

  VkViewport viewports[] = { viewport };
  VkRect2D   scissors[]  = { scissor };

  VkPipelineViewportStateCreateInfo viewport_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .scissorCount = 1,
      .pViewports = viewports,
      .pScissors = scissors,
  };

  VkPipelineRasterizationStateCreateInfo rasterizer_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .depthBiasEnable = VK_FALSE,
      .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisample_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .sampleShadingEnable = VK_FALSE,
      .pSampleMask = NULL,
  };

  VkPipelineColorBlendAttachmentState color_blend_as = {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                        VK_COLOR_COMPONENT_G_BIT |
                        VK_COLOR_COMPONENT_B_BIT |
                        VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_TRUE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
      .alphaBlendOp = VK_BLEND_OP_MAX,
  };

  VkPipelineColorBlendStateCreateInfo blend_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .attachmentCount = 1,
      .pAttachments = &color_blend_as,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY, // enabled?
  };

  VkPipelineDepthStencilStateCreateInfo depth_stencil_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
    .depthTestEnable = VK_TRUE,
    .depthWriteEnable = VK_TRUE,
    .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
    .depthBoundsTestEnable = VK_FALSE,
    .back.failOp = VK_STENCIL_OP_KEEP,
    .back.passOp = VK_STENCIL_OP_KEEP,
    .back.compareOp = VK_COMPARE_OP_ALWAYS,
    .stencilTestEnable = VK_FALSE,
  };
  depth_stencil_ci.front = depth_stencil_ci.back;

  VkPipelineCacheCreateInfo pipeline_cache_ci = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
  };

  ONL_VK_CHECK_EXIT(vkCreatePipelineCache(onl_vk_device,
                                          &pipeline_cache_ci,
                                          0,
                                          &pipeline_cache));

  VkGraphicsPipelineCreateInfo graphics_pipeline_ci = {
    .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
    .pViewportState = &viewport_state_ci,
    .pRasterizationState = &rasterizer_state_ci,
    .pMultisampleState = &multisample_ci,
    .pColorBlendState = &blend_state_ci,
    .pDepthStencilState = &depth_stencil_ci,
    .stageCount = 2,
    .pStages = shader_stages_ci,
    .pVertexInputState = &onl_vk_vertex_input_state_ci,
    .pInputAssemblyState = &input_assembly_state_ci,
    .layout = onl_vk_pipeline_layout,
    .renderPass = render_pass,
    .subpass = 0,
  };

  ONL_VK_CHECK_EXIT(vkCreateGraphicsPipelines(onl_vk_device,
                                              pipeline_cache,
                                              1,
                                              &graphics_pipeline_ci,
                                              0,
                                              &pipeline));

  vkDestroyShaderModule(onl_vk_device, onl_vk_frag_shader_module, NULL);
  vkDestroyShaderModule(onl_vk_device, onl_vk_vert_shader_module, NULL);
}

void onl_vk_prepare_framebuffers(bool restart) {

    VkFramebufferCreateInfo fb_ci = {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .pNext = 0,
        .renderPass = render_pass,
        .attachmentCount = 2,
        .width =  swap_width,
        .height = swap_height,
        .layers = 1,
    };

    for (uint32_t i = 0; i < onl_vk_max_img; i++) {

        VkImageView attachments[] = {
          sbs_render? color.image_view:
                      swapchain_bits[i].image_view,
          depth.image_view,
        };
        fb_ci.pAttachments = attachments;

        VkResult err = vkCreateFramebuffer(onl_vk_device, &fb_ci, 0,
                                           &swapchain_bits[i].framebuffer);
        assert(!err);
    }
}

VkCommandBuffer onl_vk_begin_cmd_buf(uint32_t ii) {

  vkWaitForFences(onl_vk_device, 1, &swapchain_bits[ii].cmd_buf_fence, VK_TRUE, UINT64_MAX);

  VkCommandBuffer cmd_buf = swapchain_bits[ii].cmd_buf;

  const VkCommandBufferBeginInfo cmd_buf_bi = {
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
    .pInheritanceInfo = NULL,
    .pNext = 0,
  };

  ONL_VK_CHECK_EXIT(vkBeginCommandBuffer(cmd_buf, &cmd_buf_bi));

  return cmd_buf;
}

void onl_vk_begin_render_pass(uint32_t ii, VkCommandBuffer cmd_buf) {

  const VkClearValue clear_values[] = {
    { .color.float32 = { 0.2f, 0.8f, 1.0f, 0.0f } },
    { .depthStencil = { 1.0f, 0 }},
  };

  const VkRenderPassBeginInfo render_pass_bi = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = render_pass,
      .framebuffer = swapchain_bits[ii].framebuffer,
      .renderArea.offset = { 0, 0 },
      .renderArea.extent = swapchain_extent,
      .clearValueCount = 2,
      .pClearValues = clear_values,
      .pNext = 0,
  };

  vkCmdBeginRenderPass(cmd_buf, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
}

void onl_vk_end_cmd_buf_and_render_pass(uint32_t ii, VkCommandBuffer cmd_buf){

  vkCmdEndRenderPass(cmd_buf);

  if(sbs_render) copy_colour_to_swap(ii);

  ONL_VK_CHECK_EXIT(vkEndCommandBuffer(cmd_buf));
}

void onl_vk_finish_rendering() {

  vkDeviceWaitIdle(onl_vk_device);

  for (uint32_t i = 0; i < onl_vk_max_img; i++) {
    vkWaitForFences(onl_vk_device, 1, &swapchain_bits[i].cmd_buf_fence, VK_TRUE, UINT64_MAX);
    vkDestroyFence(onl_vk_device, swapchain_bits[i].cmd_buf_fence, NULL);
  }

  vkDestroyPipeline(onl_vk_device, pipeline, NULL);
  vkDestroyPipelineCache(onl_vk_device, pipeline_cache, NULL);

  // ---------------------------------

  vkDestroyImageView(onl_vk_device, depth.image_view, NULL);
  vkDestroyImage(onl_vk_device, depth.image, NULL);
  vkFreeMemory(onl_vk_device, depth.device_memory, NULL);

  if(sbs_render){
    vkDestroyImageView(onl_vk_device, color.image_view, NULL);
    vkDestroyImage(onl_vk_device, color.image, NULL);
    vkFreeMemory(onl_vk_device, color.device_memory, NULL);
  }

  uint32_t i;
  if (swapchain_bits) {
     for (i = 0; i < onl_vk_max_img; i++) {
         vkFreeCommandBuffers(onl_vk_device, command_pool, 1, &swapchain_bits[i].cmd_buf);
         vkDestroyFramebuffer(onl_vk_device, swapchain_bits[i].framebuffer, NULL);
         vkDestroyImageView(onl_vk_device, swapchain_bits[i].image_view, NULL);
     }
     free(swapchain_bits);
  }

  // ---------------------------------

  VK_DESTROY(vkDestroySemaphore, onl_vk_device, image_acquired_semaphore);
  VK_DESTROY(vkDestroySemaphore, onl_vk_device, render_complete_semaphore);

  vkDestroyRenderPass(onl_vk_device, render_pass, NULL);
}


