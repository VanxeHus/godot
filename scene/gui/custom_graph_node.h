/**************************************************************************/
/*  custom_graph_node.h                                                          */
/**************************************************************************/

#ifndef CUSTOM_GRAPH_NODE_H
#define CUSTOM_GRAPH_NODE_H

#include "scene/gui/container.h"

class CustomGraphNode : public Container {
	GDCLASS(CustomGraphNode, Container);

public:
	enum Overlay {
		OVERLAY_DISABLED,
		OVERLAY_BREAKPOINT,
		OVERLAY_POSITION
	};

private:
	struct Slot {
		bool enable;
		int type;
		Color color;
		Ref<Texture> custom_slot;
		Vector2 offset;

		Slot() {
			enable = false;
			type = 0;
			color = Color(1, 1, 1, 1);
			offset = Vector2();
		}
	};

	String title;
	bool show_close;
	bool alway_on_top = false;
	Vector2 offset;
	bool comment;
	bool resizable;
	bool dragged = false;
	int slotNum = 0;

	bool resizing;
	Vector2 resizing_from;
	Vector2 resizing_from_size;

	Rect2 close_rect;

	Vector<int> cache_y;

	struct ConnCache {
		Vector2 pos;
		int type;
		Color color;
	};

	Vector<ConnCache> conn_cache;

	Map<int, Slot> slot_info;

	bool connpos_dirty;

	void _connpos_update();
	void _resort();

	Vector2 drag_from;
	bool selected;

	Overlay overlay;

protected:
	void _gui_input(const Ref<InputEvent> &p_ev);
	void _notification(int p_what);
	static void _bind_methods();

	bool _set(const StringName &p_name, const Variant &p_value);
	bool _get(const StringName &p_name, Variant &r_ret) const;
	void _get_property_list(List<PropertyInfo> *p_list) const;

public:

	bool has_point(const Point2 &p_point) const;

	void set_slot(int p_idx, bool p_enable, int p_type, const Color &p_color, const Vector2 & p_offset, const Ref<Texture> &p_custom = Ref<Texture>());
	void clear_slot(int p_idx);
	void clear_all_slots();

	bool is_slot_enabled(int p_idx) const;
	void set_slot_enabled(int p_idx, bool p_enable);

	void set_slot_type(int p_idx, int p_type);
	int get_slot_type(int p_idx) const;

	void set_slot_color(int p_idx, const Color &p_color);
	Color get_slot_color(int p_idx) const;

	void set_slot_num(int p_value) {
		slotNum = p_value;
	}
	int get_slot_num() const {
		return slotNum;
	};


	void set_title(const String &p_title);
	String get_title() const;

	void set_alway_on_top(bool p_enable) {
		alway_on_top = p_enable;
	}
	bool is_alway_on_top() const {
		return alway_on_top;
	}

	void set_offset(const Vector2 &p_offset);
	Vector2 get_offset() const;

	void set_selected(bool p_selected);
	bool is_selected();

	void set_drag(bool p_drag);
	bool get_drag() { return dragged; }
	Vector2 get_drag_from();

	void set_show_close_button(bool p_enable);
	bool is_close_button_visible() const;

	int get_connection_count();
	Vector2 get_connection_position(int p_idx);
	int get_connection_type(int p_idx);
	Color get_connection_color(int p_idx);

	void set_overlay(Overlay p_overlay);
	Overlay get_overlay() const;

	void set_comment(bool p_enable);
	bool is_comment() const;

	void set_resizable(bool p_enable);
	bool is_resizable() const;

	virtual Size2 get_minimum_size() const;

	bool is_resizing() const { return resizing; }

	CustomGraphNode();
};

VARIANT_ENUM_CAST(CustomGraphNode::Overlay)

#endif // CUSTOM_GRAPH_NODE_H
