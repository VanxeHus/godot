/**************************************************************************/
/*  custom_graph_element.cpp                                                     */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "custom_graph_element.h"

#include "core/string/translation.h"
#include "scene/gui/custom_graph_edit.h"
#include "scene/theme/theme_db.h"

#ifdef TOOLS_ENABLED
void CustomGraphElement::_edit_set_position(const Point2 &p_position) {
	CustomGraphEdit *graph = Object::cast_to<CustomGraphEdit>(get_parent());
	if (graph) {
		Point2 offset = (p_position + graph->get_scroll_offset()) * graph->get_zoom();
		set_position_offset(offset);
	}
	set_position(p_position);
}
#endif

void CustomGraphElement::_resort() {
	Size2 size = get_size();

	for (int i = 0; i < get_child_count(); i++) {
		Control *child = Object::cast_to<Control>(get_child(i));
		if (!child || !child->is_visible_in_tree()) {
			continue;
		}
		if (child->is_set_as_top_level()) {
			continue;
		}

		fit_child_in_rect(child, Rect2(Point2(), size));
	}
}

Size2 CustomGraphElement::get_minimum_size() const {
	Size2 minsize;
	for (int i = 0; i < get_child_count(); i++) {
		Control *child = Object::cast_to<Control>(get_child(i));
		if (!child) {
			continue;
		}
		if (child->is_set_as_top_level()) {
			continue;
		}

		Size2i size = child->get_combined_minimum_size();

		minsize.width = MAX(minsize.width, size.width);
		minsize.height = MAX(minsize.height, size.height);
	}

	return minsize;
}

void CustomGraphElement::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_SORT_CHILDREN: {
			_resort();
		} break;

		case NOTIFICATION_LAYOUT_DIRECTION_CHANGED:
		case NOTIFICATION_TRANSLATION_CHANGED:
		case NOTIFICATION_THEME_CHANGED: {
			update_minimum_size();
			queue_redraw();
		} break;
	}
}

void CustomGraphElement::_validate_property(PropertyInfo &p_property) const {
	Control::_validate_property(p_property);
	CustomGraphEdit *graph = Object::cast_to<CustomGraphEdit>(get_parent());
	if (graph) {
		if (p_property.name == "position") {
			p_property.usage |= PROPERTY_USAGE_READ_ONLY;
		}
	}
}

void CustomGraphElement::set_position_offset(const Vector2 &p_offset) {
	if (position_offset == p_offset) {
		return;
	}

	position_offset = p_offset;
	emit_signal(SNAME("position_offset_changed"));
	queue_redraw();
}

Vector2 CustomGraphElement::get_position_offset() const {
	return position_offset;
}

void CustomGraphElement::set_selected(bool p_selected) {
	if (!is_selectable() || selected == p_selected) {
		return;
	}
	selected = p_selected;
	emit_signal(p_selected ? SNAME("node_selected") : SNAME("node_deselected"));
	queue_redraw();
}

bool CustomGraphElement::is_selected() {
	return selected;
}

void CustomGraphElement::set_drag(bool p_drag) {
	if (p_drag) {
		drag_from = get_position_offset();
	} else {
		emit_signal(SNAME("dragged"), drag_from, get_position_offset()); // Required for undo/redo.
	}
}

Vector2 CustomGraphElement::get_drag_from() {
	return drag_from;
}

void CustomGraphElement::gui_input(const Ref<InputEvent> &p_ev) {
	ERR_FAIL_COND(p_ev.is_null());

	Ref<InputEventMouseButton> mb = p_ev;
	if (mb.is_valid()) {
		ERR_FAIL_NULL_MSG(get_parent_control(), "CustomGraphElement must be the child of a CustomGraphEdit node.");

		if (mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			Vector2 mpos = mb->get_position();

			if (resizable && mpos.x > get_size().x - theme_cache.resizer->get_width() && mpos.y > get_size().y - theme_cache.resizer->get_height()) {
				resizing = true;
				resizing_from = mpos;
				resizing_from_size = get_size();
				accept_event();
				return;
			}

			emit_signal(SNAME("raise_request"));
		}

		if (!mb->is_pressed() && mb->get_button_index() == MouseButton::LEFT) {
			resizing = false;
		}
	}

	Ref<InputEventMouseMotion> mm = p_ev;
	if (resizing && mm.is_valid()) {
		Vector2 mpos = mm->get_position();
		Vector2 diff = mpos - resizing_from;

		emit_signal(SNAME("resize_request"), resizing_from_size + diff);
	}
}

void CustomGraphElement::set_resizable(bool p_enable) {
	if (resizable == p_enable) {
		return;
	}
	resizable = p_enable;
	queue_redraw();
}

bool CustomGraphElement::is_resizable() const {
	return resizable;
}

void CustomGraphElement::set_draggable(bool p_draggable) {
	draggable = p_draggable;
}

bool CustomGraphElement::is_draggable() {
	return draggable;
}

void CustomGraphElement::set_selectable(bool p_selectable) {
	if (!p_selectable) {
		set_selected(false);
	}
	selectable = p_selectable;
}

bool CustomGraphElement::is_selectable() {
	return selectable;
}

bool CustomGraphElement::has_custom_height_point(const Point2 &p_point) const {
	if(custom_drag_height > 0){
		return !(Rect2(0, 0, get_size().width, custom_drag_height).has_point(p_point));
	}
	return false;
}

void CustomGraphElement::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_resizable", "resizable"), &CustomGraphElement::set_resizable);
	ClassDB::bind_method(D_METHOD("is_resizable"), &CustomGraphElement::is_resizable);

	ClassDB::bind_method(D_METHOD("set_draggable", "draggable"), &CustomGraphElement::set_draggable);
	ClassDB::bind_method(D_METHOD("is_draggable"), &CustomGraphElement::is_draggable);

	ClassDB::bind_method(D_METHOD("set_selectable", "selectable"), &CustomGraphElement::set_selectable);
	ClassDB::bind_method(D_METHOD("is_selectable"), &CustomGraphElement::is_selectable);

	ClassDB::bind_method(D_METHOD("set_selected", "selected"), &CustomGraphElement::set_selected);
	ClassDB::bind_method(D_METHOD("is_selected"), &CustomGraphElement::is_selected);

	ClassDB::bind_method(D_METHOD("set_position_offset", "offset"), &CustomGraphElement::set_position_offset);
	ClassDB::bind_method(D_METHOD("get_position_offset"), &CustomGraphElement::get_position_offset);
	ClassDB::bind_method(D_METHOD("set_custom_drag_height", "custom_drag_height"), &CustomGraphElement::set_custom_drag_height);
	ClassDB::bind_method(D_METHOD("get_custom_drag_height"), &CustomGraphElement::get_custom_drag_height);

	ADD_PROPERTY(PropertyInfo(Variant::VECTOR2, "position_offset"), "set_position_offset", "get_position_offset");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "resizable"), "set_resizable", "is_resizable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "draggable"), "set_draggable", "is_draggable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selectable"), "set_selectable", "is_selectable");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "selected"), "set_selected", "is_selected");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "custom_drag_height"), "set_custom_drag_height", "get_custom_drag_height");

	ADD_SIGNAL(MethodInfo("dragged", PropertyInfo(Variant::VECTOR2, "from"), PropertyInfo(Variant::VECTOR2, "to")));
	ADD_SIGNAL(MethodInfo("node_selected"));
	ADD_SIGNAL(MethodInfo("node_deselected"));

	ADD_SIGNAL(MethodInfo("raise_request"));
	ADD_SIGNAL(MethodInfo("delete_request"));
	ADD_SIGNAL(MethodInfo("resize_request", PropertyInfo(Variant::VECTOR2, "new_minsize")));

	ADD_SIGNAL(MethodInfo("position_offset_changed"));

	BIND_THEME_ITEM(Theme::DATA_TYPE_ICON, CustomGraphElement, resizer);
}
