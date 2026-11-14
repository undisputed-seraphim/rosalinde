#pragma once

class DebugUI {
public:
	DebugUI();
	~DebugUI();

	void begin_frame();
	void end_frame();

	void set_stats_visible(bool visible) noexcept { _stats_visible = visible; }
	void toggle_stats() noexcept { _stats_visible = !_stats_visible; }
	void draw_stats(float fps, float frame_ms, bool vsync);

private:
	bool _stats_visible = false;
};
