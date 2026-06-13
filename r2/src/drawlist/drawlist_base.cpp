#include <r2/drawlist/drawlist_base.h>
#include <r2/renderer.h>


r2_begin_

#ifdef _DEBUG
void drawlist_base::assert_render_thread()
{
    renderer_->assert_render_thread();
}
#endif // _DEBUG

r2_end_