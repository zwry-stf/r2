#pragma once
#include <backend/object.h>
#include <backend/textureview.h>
#include <memory>


r2_begin_

struct render_target_attachment {
    textureview* view;
};

struct framebuffer_desc {
    render_target_attachment color_attachment{};
    render_target_attachment depth_attachment{};
};

class framebuffer : public object<framebuffer_desc> {
protected:
	using object<framebuffer_desc>::object;
};

r2_end_