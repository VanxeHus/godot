/**************************************************************************/
/*  custom_graph_node.h                                                          */
/**************************************************************************/

#include "custom_graph_node.h"

#include "core/method_bind_ext.gen.inc"

struct _MinSizeCache {
	int min_size;
	bool will_stretch;
	int final_size;
};

bool CustomGraphNode::_set(const StringName &p_name, const Variant &p_value) {
	if (!p_name.operator String().begins_with("slot/")) {
		return false;
	}

	int idx = p_name.operator String().get_slice("/", 1).to_int();
	String what = p_name.operator String().get_slice("/", 2);

	Slot si;
	if (slot_info.has(idx)) {
		si = slot_info[idx];
	}

	if (what == "enable") {
		si.enable = p_value;
	} else if (what == "type") {
		si.type = p_value;
	} else if (what == "color") {
		si.color = p_value;
	} else if (what == "offset") {
		si.offset = p_value;
	} else {
		return false;
	}

	set_slot(idx, si.enable, si.type, si.color, si.offset);
	update();
	return true;
}

bool CustomGraphNode::_get(const StringName &p_name, Variant &r_ret) const {
	if (!p_name.operator String().begins_with("slot/")) {
		return false;
	}

	int idx = p_name.operator String().get_slice("/", 1).to_int();
	String what = p_name.operator String().get_slice("/", 2);

	Slot si;
	if (slot_info.has(idx)) {
		si = slot_info[idx];
	}

	if (what == "enable") {
		r_ret = si.enable;
	} else if (what == "type") {
		r_ret = si.type;
	} else if (what == "color") {
		r_ret = si.color;
	} else if (what == "offset") {
		r_ret = si.offset;
	} else {
		return false;
	}

	return true;
}
void CustomGraphNode::_get_property_list(List<PropertyInfo> *p_list) const {
	int idx = 0;
	for (int i = 0; i < this->get_slot_num(); i++) {
		String base = "slot/" + itos(idx) + "/";

		p_list->push_back(PropertyInfo(Variant::BOOL, base + "enable"));
		p_list->push_back(PropertyInfo(Variant::INT, base + "type"));
		p_list->push_back(PropertyInfo(Variant::COLOR, base + "color"));
		p_list->push_back(PropertyInfo(Variant::VECTOR2, base + "offset"));

		idx++;
	}
}

void CustomGraphNode::_resort() {
	/** First pass, determine minimum size AND amount of stretchable elements */

	Size2 new_size = get_size();
	Ref<StyleBox> sb = get_stylebox("frame");

	int sep = get_constant("separation");

	bool first = true;
	int children_count = 0;
	int stretch_min = 0;
	int stretch_avail = 0;
	float stretch_ratio_total = 0;
	Map<Control *, _MinSizeCache> min_size_cache;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c || !c->is_visible_in_tree()) {
			continue;
		}
		if (c->is_set_as_toplevel()) {
			continue;
		}

		Size2i size = c->get_combined_minimum_size();
		_MinSizeCache msc;

		stretch_min += size.height;
		msc.min_size = size.height;
		msc.will_stretch = c->get_v_size_flags() & SIZE_EXPAND;

		if (msc.will_stretch) {
			stretch_avail += msc.min_size;
			stretch_ratio_total += c->get_stretch_ratio();
		}
		msc.final_size = msc.min_size;
		min_size_cache[c] = msc;
		children_count++;
	}

	if (children_count == 0) {
		return;
	}

	int stretch_max = new_size.height - (children_count - 1) * sep;
	int stretch_diff = stretch_max - stretch_min;
	if (stretch_diff < 0) {
		//avoid negative stretch space
		stretch_diff = 0;
	}

	stretch_avail += stretch_diff - sb->get_margin(MARGIN_BOTTOM) - sb->get_margin(MARGIN_TOP); //available stretch space.
	/** Second, pass successively to discard elements that can't be stretched, this will run while stretchable
		elements exist */

	while (stretch_ratio_total > 0) { // first of all, don't even be here if no stretchable objects exist
		bool refit_successful = true; //assume refit-test will go well

		for (int i = 0; i < get_child_count(); i++) {
			Control *c = Object::cast_to<Control>(get_child(i));
			if (!c || !c->is_visible_in_tree()) {
				continue;
			}
			if (c->is_set_as_toplevel()) {
				continue;
			}

			ERR_FAIL_COND(!min_size_cache.has(c));
			_MinSizeCache &msc = min_size_cache[c];

			if (msc.will_stretch) { //wants to stretch
				//let's see if it can really stretch

				int final_pixel_size = stretch_avail * c->get_stretch_ratio() / stretch_ratio_total;
				if (final_pixel_size < msc.min_size) {
					//if available stretching area is too small for widget,
					//then remove it from stretching area
					msc.will_stretch = false;
					stretch_ratio_total -= c->get_stretch_ratio();
					refit_successful = false;
					stretch_avail -= msc.min_size;
					msc.final_size = msc.min_size;
					break;
				} else {
					msc.final_size = final_pixel_size;
				}
			}
		}

		if (refit_successful) { //uf refit went well, break
			break;
		}
	}

	/** Final pass, draw and stretch elements **/

	int ofs = sb->get_margin(MARGIN_TOP);

	first = true;
	int idx = 0;
	// cache_y.clear();
	int w = new_size.width - sb->get_minimum_size().x;

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c || !c->is_visible_in_tree()) {
			continue;
		}
		if (c->is_set_as_toplevel()) {
			continue;
		}

		_MinSizeCache &msc = min_size_cache[c];

		if (first) {
			first = false;
		} else {
			ofs += sep;
		}

		int from = ofs;
		int to = ofs + msc.final_size;

		if (msc.will_stretch && idx == children_count - 1) {
			//adjust so the last one always fits perfect
			//compensating for numerical imprecision

			to = new_size.height - sb->get_margin(MARGIN_BOTTOM);
		}

		int size = to - from;

		Rect2 rect(sb->get_margin(MARGIN_LEFT), from, w, size);

		fit_child_in_rect(c, rect);
		// cache_y.push_back(from - sb->get_margin(MARGIN_TOP) + size * 0.5);

		ofs = to;
		idx++;
	}

	update();
	connpos_dirty = true;
}

bool CustomGraphNode::has_point(const Point2 &p_point) const {
	if (comment) {
		Ref<StyleBox> comment = get_stylebox("comment");
		Ref<Texture> resizer = get_icon("resizer");

		if (Rect2(get_size() - resizer->get_size(), resizer->get_size()).has_point(p_point)) {
			return true;
		}

		if (Rect2(0, 0, get_size().width, comment->get_margin(MARGIN_TOP)).has_point(p_point)) {
			return true;
		}

		return false;

	} else {
		return Control::has_point(p_point);
	}
}

void CustomGraphNode::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_DRAW: {
			Ref<StyleBox> sb;

			if (comment) {
				sb = get_stylebox(selected ? "commentfocus" : "comment");

			} else {
				sb = get_stylebox(selected ? "selectedframe" : "frame");
			}

			//sb=sb->duplicate();
			//sb->call("set_modulate",modulate);
			Ref<Texture> port = get_icon("port");
			Ref<Texture> close = get_icon("close");
			Ref<Texture> resizer = get_icon("resizer");
			int close_offset = get_constant("close_offset");
			int close_h_offset = get_constant("close_h_offset");
			Color close_color = get_color("close_color");
			Color resizer_color = get_color("resizer_color");
			Ref<Font> title_font = get_font("title_font");
			int title_offset = get_constant("title_offset");
			int title_h_offset = get_constant("title_h_offset");
			Color title_color = get_color("title_color");
			Point2i icofs = -port->get_size() * 0.5;
			int edgeofs = get_constant("port_offset");
			icofs.y += sb->get_margin(MARGIN_TOP);

			draw_style_box(sb, Rect2(Point2(), get_size()));

			switch (overlay) {
				case OVERLAY_DISABLED: {
				} break;
				case OVERLAY_BREAKPOINT: {
					draw_style_box(get_stylebox("breakpoint"), Rect2(Point2(), get_size()));
				} break;
				case OVERLAY_POSITION: {
					draw_style_box(get_stylebox("position"), Rect2(Point2(), get_size()));

				} break;
			}

			int w = get_size().width - sb->get_minimum_size().x;

			if (show_close) {
				w -= close->get_width();
			}

			draw_string(title_font, Point2(sb->get_margin(MARGIN_LEFT) + title_h_offset, -title_font->get_height() + title_font->get_ascent() + title_offset), title, title_color, w);
			if (show_close) {
				Vector2 cpos = Point2(w + sb->get_margin(MARGIN_LEFT) + close_h_offset, -close->get_height() + close_offset);
				draw_texture(close, cpos, close_color);
				close_rect.position = cpos;
				close_rect.size = close->get_size();
			} else {
				close_rect = Rect2();
			}

			for (Map<int, Slot>::Element *E = slot_info.front(); E; E = E->next()) {
				if (E->key() < 0 || E->key() >= get_slot_num()) {
					continue;
				}
				if (!slot_info.has(E->key())) {
					continue;
				}
				const Slot &s = slot_info[E->key()];
				//left
				if (s.enable) {
					Ref<Texture> p = port;
					if (s.custom_slot.is_valid()) {
						p = s.custom_slot;
					}
					p->draw(get_canvas_item(), Point2(s.offset.x, s.offset.y), s.color);
				}

			}

			if (resizable) {
				draw_texture(resizer, get_size() - resizer->get_size(), resizer_color);
			}
		} break;

		case NOTIFICATION_SORT_CHILDREN: {
			_resort();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			minimum_size_changed();
		} break;
	}
}

void CustomGraphNode::set_slot(int p_idx, bool p_enable, int p_type, const Color &p_color, const Vector2 &p_offset, const Ref<Texture> &p_custom) {
	ERR_FAIL_COND_MSG(p_idx < 0, vformat("Cannot set slot with p_idx (%d) lesser than zero.", p_idx));

	if (!p_enable && p_type == 0 && p_color == Color(1, 1, 1, 1)) {
		slot_info.erase(p_idx);
		return;
	}

	Slot s;
	s.enable = p_enable;
	s.type = p_type;
	s.color = p_color;
	s.custom_slot = p_custom;
	s.offset = p_offset;
	slot_info[p_idx] = s;
	update();
	connpos_dirty = true;

	emit_signal("slot_updated", p_idx);
}

void CustomGraphNode::clear_slot(int p_idx) {
	slot_info.erase(p_idx);
	update();
	connpos_dirty = true;
}
void CustomGraphNode::clear_all_slots() {
	slot_info.clear();
	update();
	connpos_dirty = true;
}
bool CustomGraphNode::is_slot_enabled(int p_idx) const {
	if (!slot_info.has(p_idx)) {
		return false;
	}
	return slot_info[p_idx].enable;
}

void CustomGraphNode::set_slot_enabled(int p_idx, bool p_enable) {
	ERR_FAIL_COND_MSG(p_idx < 0, vformat("Cannot set enable for the slot with p_idx (%d) lesser than zero.", p_idx));

	slot_info[p_idx].enable = p_enable;
	update();
	connpos_dirty = true;

	emit_signal("slot_updated", p_idx);
}

void CustomGraphNode::set_slot_type(int p_idx, int p_type) {
	ERR_FAIL_COND_MSG(!slot_info.has(p_idx), vformat("Cannot set type for the slot '%d' because it hasn't been enabled.", p_idx));

	slot_info[p_idx].type = p_type;
	update();
	connpos_dirty = true;

	emit_signal("slot_updated", p_idx);
}

int CustomGraphNode::get_slot_type(int p_idx) const {
	if (!slot_info.has(p_idx)) {
		return 0;
	}
	return slot_info[p_idx].type;
}

void CustomGraphNode::set_slot_color(int p_idx, const Color &p_color) {
	ERR_FAIL_COND_MSG(!slot_info.has(p_idx), vformat("Cannot set color for the slot '%d' because it hasn't been enabled.", p_idx));

	slot_info[p_idx].color = p_color;
	update();
	connpos_dirty = true;

	emit_signal("slot_updated", p_idx);
}

Color CustomGraphNode::get_slot_color(int p_idx) const {
	if (!slot_info.has(p_idx)) {
		return Color(1, 1, 1, 1);
	}
	return slot_info[p_idx].color;
}

Size2 CustomGraphNode::get_minimum_size() const {
	Ref<Font> title_font = get_font("title_font");

	int sep = get_constant("separation");
	Ref<StyleBox> sb = get_stylebox("frame");
	bool first = true;

	Size2 minsize;
	minsize.x = title_font->get_string_size(title).x;
	if (show_close) {
		Ref<Texture> close = get_icon("close");
		minsize.x += sep + close->get_width();
	}

	for (int i = 0; i < get_child_count(); i++) {
		Control *c = Object::cast_to<Control>(get_child(i));
		if (!c) {
			continue;
		}
		if (c->is_set_as_toplevel()) {
			continue;
		}

		Size2i size = c->get_combined_minimum_size();

		minsize.y += size.y;
		minsize.x = MAX(minsize.x, size.x);

		if (first) {
			first = false;
		} else {
			minsize.y += sep;
		}
	}

	return minsize + sb->get_minimum_size();
}

void CustomGraphNode::set_title(const String &p_title) {
	if (title == p_title) {
		return;
	}
	title = p_title;
	update();
	_change_notify("title");
	minimum_size_changed();
}

String CustomGraphNode::get_title() const {
	return title;
}

void CustomGraphNode::set_offset(const Vector2 &p_offset) {
	offset = p_offset;
	emit_signal("offset_changed");
	update();
}

Vector2 CustomGraphNode::get_offset() const {
	return offset;
}

void CustomGraphNode::set_selected(bool p_selected) {
	selected = p_selected;
	update();
}

bool CustomGraphNode::is_selected() {
	return selected;
}

void CustomGraphNode::set_drag(bool p_drag) {
	if (p_drag) {
		drag_from = get_offset();
	} else {
		emit_signal("dragged", drag_from, get_offset()); //useful for undo/redo
	}
}

Vector2 CustomGraphNode::get_drag_from() {
	return drag_from;
}

void CustomGraphNode::set_show_close_button(bool p_enable) {
	show_close = p_enable;
	update();
}
bool CustomGraphNode::is_close_button_visible() const {
	return show_close;
}

void CustomGraphNode::_connpos_update() {
	int edgeofs = get_constant("port_offset");
	int sep = get_constant("separation");

	Ref<StyleBox> sb = get_stylebox("frame");
	conn_cache.clear();

	int idx = 0;
	Ref<Texture> port = get_icon("port");
	Point2i icofs = -port->get_size() * 0.5;

	for (int i = 0; i < get_slot_num(); i++) {
		int y = sb->get_margin(MARGIN_TOP);

		if (slot_info.has(idx)) {
			const Slot &tmpSlot = slot_info[idx];
			if (tmpSlot.enable) {
				ConnCache cc;
				cc.pos = Point2i(tmpSlot.offset.x - icofs.x, y + tmpSlot.offset.y - icofs.y);
				cc.type = slot_info[idx].type;
				cc.color = slot_info[idx].color;
				conn_cache.push_back(cc);
			}
		}

		idx++;
	}

	connpos_dirty = false;
}

int CustomGraphNode::get_connection_count() {
	if (connpos_dirty) {
		_connpos_update();
	}

	return conn_cache.size();
}

Vector2 CustomGraphNode::get_connection_position(int p_idx) {
	if (connpos_dirty) {
		_connpos_update();
	}

	ERR_FAIL_INDEX_V(p_idx, conn_cache.size(), Vector2());
	Vector2 pos = conn_cache[p_idx].pos;
	pos.x *= get_scale().x;
	pos.y *= get_scale().y;
	return pos;
}

int CustomGraphNode::get_connection_type(int p_idx) {
	if (connpos_dirty) {
		_connpos_update();
	}

	ERR_FAIL_INDEX_V(p_idx, conn_cache.size(), 0);
	return conn_cache[p_idx].type;
}

Color CustomGraphNode::get_connection_color(int p_idx) {
	if (connpos_dirty) {
		_connpos_update();
	}

	ERR_FAIL_INDEX_V(p_idx, conn_cache.size(), Color());
	return conn_cache[p_idx].color;
}

void CustomGraphNode::_gui_input(const Ref<InputEvent> &p_ev) {
	if (!(this->is_visible())){
		return;
	}
	Ref<InputEventMouseButton> mb = p_ev;
	if (mb.is_valid()) {
		ERR_FAIL_COND_MSG(get_parent_control() == nullptr, "CustomGraphNode must be the child of a GraphEdit node.");

		if (mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
			Vector2 mpos = Vector2(mb->get_position().x, mb->get_position().y);
			if (close_rect.size != Size2() && close_rect.has_point(mpos)) {
				//send focus to parent
				get_parent_control()->grab_focus();
				emit_signal("close_request");
				accept_event();
				return;
			}

			Ref<Texture> resizer = get_icon("resizer");

			if (resizable && mpos.x > get_size().x - resizer->get_width() && mpos.y > get_size().y - resizer->get_height()) {
				resizing = true;
				resizing_from = mpos;
				resizing_from_size = get_size();
				accept_event();
				return;
			}

			emit_signal("raise_request");
		}

		if (!mb->is_pressed() && mb->get_button_index() == BUTTON_LEFT) {
			resizing = false;
		}
	}

	Ref<InputEventMouseMotion> mm = p_ev;
	if (resizing && mm.is_valid()) {
		Vector2 mpos = mm->get_position();

		Vector2 diff = mpos - resizing_from;

		emit_signal("resize_request", resizing_from_size + diff);
	}
}

void CustomGraphNode::set_overlay(Overlay p_overlay) {
	overlay = p_overlay;
	update();
}

CustomGraphNode::Overlay CustomGraphNode::get_overlay() const {
	return overlay;
}

void CustomGraphNode::set_comment(bool p_enable) {
	comment = p_enable;
	update();
}

bool CustomGraphNode::is_comment() const {
	return comment;
}

void CustomGraphNode::set_resizable(bool p_enable) {
	resizable = p_enable;
	update();
}

bool CustomGraphNode::is_resizable() const {
	return resizable;
}

void CustomGraphNode::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_title", "title"), &CustomGraphNode::set_title);
	ClassDB::bind_method(D_METHOD("get_title"), &CustomGraphNode::get_title);

	ClassDB::bind_method(D_METHOD("set_slot_num", "slotNum"), &CustomGraphNode::set_slot_num);
	ClassDB::bind_method(D_METHOD("get_slot_num"), &CustomGraphNode::get_slot_num);

	ClassDB::bind_method(D_METHOD("_gui_input"), &CustomGraphNode::_gui_input);

	ClassDB::bind_method(D_METHOD("set_slot", "idx", "enable", "type", "color", "offset", "custom"), &CustomGraphNode::set_slot, DEFVAL(Vector2()), DEFVAL(Ref<Texture>()));
	ClassDB::bind_method(D_METHOD("clear_slot", "idx"), &CustomGraphNode::clear_slot);
	ClassDB::bind_method(D_METHOD("clear_all_slots"), &CustomGraphNode::clear_all_slots);

	ClassDB::bind_method(D_METHOD("is_slot_enabled", "idx"), &CustomGraphNode::is_slot_enabled);
	ClassDB::bind_method(D_METHOD("set_slot_enabled", "idx", "enable"), &CustomGraphNode::set_slot_enabled);

	ClassDB::bind_method(D_METHOD("set_slot_type", "idx", "type"), &CustomGraphNode::set_slot_type);
	ClassDB::bind_method(D_METHOD("get_slot_type", "idx"), &CustomGraphNode::get_slot_type);

	ClassDB::bind_method(D_METHOD("set_slot_color", "idx", "color"), &CustomGraphNode::set_slot_color);
	ClassDB::bind_method(D_METHOD("get_slot_color", "idx"), &CustomGraphNode::get_slot_color);

	ClassDB::bind_method(D_METHOD("set_offset", "offset"), &CustomGraphNode::set_offset);
	ClassDB::bind_method(D_METHOD("get_offset"), &CustomGraphNode::get_offset);

	ClassDB::bind_method(D_METHOD("set_comment", "comment"), &CustomGraphNode::set_comment);
	ClassDB::bind_method(D_METHOD("is_comment"), &CustomGraphNode::is_comment);

	ClassDB::bind_method(D_METHOD("set_resizable", "resizable"), &CustomGraphNode::set_resizable);
	ClassDB::bind_method(D_METHOD("is_resizable"), &CustomGraphNode::is_resizable);

	ClassDB::bind_method(D_METHOD("set_selected", "selected"), &CustomGraphNode::set_selected);
	ClassDB::bind_method(D_METHOD("is_selected"), &CustomGraphNode::is_selected);

	ClassDB::bind_method(D_METHOD("set_alway_on_top", "alway_on_top"), &CustomGraphNode::set_alway_on_top);
	ClassDB::bind_method(D_METHOD("is_alway_on_top"), &CustomGraphNode::is_alway_on_top);

	ClassDB::bind_method(D_METHOD("get_connection_count"), &CustomGraphNode::get_connection_count);

	ClassDB::bind_method(D_METHOD("get_connection_position", "idx"), &CustomGraphNode::get_connection_position);
	ClassDB::bind_method(D_METHOD("get_connection_type", "idx"), &CustomGraphNode::get_connection_type);
	ClassDB::bind_method(D_METHOD("get_connection_olor", "idx"), &CustomGraphNode::get_connection_color);

	ClassDB::bind_method(D_METHOD("set_show_close_button", "show"), &CustomGraphNode::set_show_close_button);
	ClassDB::bind_method(D_METHOD("is_close_button_visible"), &CustomGraphNode::is_close_button_visible);

	ClassDB::bind_method(D_METHOD("set_overlay", "overlay"), &CustomGraphNode::set_overlay);
	ClassDB::bind_method(D_METHOD("get_overlay"), &CustomGraphNode::get_overlay);

	ADD_PROPERTY(PropertyInfo(Variant::STRING, "title"), "set_title", "get_title");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "slotNum"), "set_slot_num", "get_slot_num");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "alway_on_top"), "set_alway_on_top", "is_alway_on_top");
	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "offset"), "set_offset", "get_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "show_close"), "set_show_close_button", "is_close_button_visible");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "resizable"), "set_resizable", "is_resizable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selected"), "set_selected", "is_selected");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "comment"), "set_comment", "is_comment");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "overlay", PROPERTY_HINT_ENUM, "Disabled,Breakpoint,Position"), "set_overlay", "get_overlay");

	ADD_SIGNAL(MethodInfo("offset_changed"));
	ADD_SIGNAL(MethodInfo("slot_updated", PropertyInfo(Variant::INT, "idx")));
	ADD_SIGNAL(MethodInfo("dragged", PropertyInfo(Variant::VECTOR2, "from"), PropertyInfo(Variant::VECTOR2, "to")));
	ADD_SIGNAL(MethodInfo("raise_request"));
	ADD_SIGNAL(MethodInfo("close_request"));
	ADD_SIGNAL(MethodInfo("resize_request", PropertyInfo(Variant::VECTOR2, "new_minsize")));

	BIND_ENUM_CONSTANT(OVERLAY_DISABLED);
	BIND_ENUM_CONSTANT(OVERLAY_BREAKPOINT);
	BIND_ENUM_CONSTANT(OVERLAY_POSITION);
}

CustomGraphNode::CustomGraphNode() {
	overlay = OVERLAY_DISABLED;
	show_close = false;
	connpos_dirty = true;
	set_mouse_filter(MOUSE_FILTER_STOP);
	comment = false;
	resizable = false;
	resizing = false;
	selected = false;
}
