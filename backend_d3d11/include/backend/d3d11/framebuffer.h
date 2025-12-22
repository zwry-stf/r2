#pragma once
#include <backend/framebuffer.h>
#include <backend/d3d11/object.h>


r2_begin_

class d3d11_framebuffer : public framebuffer,
	                      protected d3d11_object {
public:
	d3d11_framebuffer(d3d11_context* ctx, const framebuffer_desc& desc);
	~d3d11_framebuffer();
};

r2_end_