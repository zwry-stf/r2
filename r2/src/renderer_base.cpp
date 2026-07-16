#include <r2/renderer_base.h>
#include <r2/render_data.h>


r2_begin_

renderer_base::renderer_base() = default;
renderer_base::~renderer_base() {
	is_initialized_ = false;
}

error renderer_base::init(const platform_init_data& pinit, const backend_init_data& binit)
{
    context_ = r2::context::make_context(pinit, binit, true);
    if (context_->has_error()) {
        return error(
            error_code::context_initialization,
            context_->get_error(),
            context_->get_detail()
        );
    }

    return error(error_code::none);
}

error renderer_base::init(r2::context* ctx)
{
    assert(ctx != nullptr);

    context_.reset(ctx);

    return error(error_code::none);
}

r2_end_