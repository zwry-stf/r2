#include <r2/renderer_base.h>
#include <r2/render_data.h>


r2_begin_

renderer_base::renderer_base() = default;
renderer_base::~renderer_base() {
	is_initialized_ = false;
}

r2_end_