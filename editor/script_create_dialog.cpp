/*************************************************************************/
/*  script_create_dialog.cpp                                             */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2019 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2019 Godot Engine contributors (cf. AUTHORS.md)    */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "script_create_dialog.h"

#include "core/io/resource_saver.h"
#include "core/os/file_access.h"
#include "core/project_settings.h"
#include "core/script_language.h"
#include "editor/create_dialog.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor_file_system.h"

void ScriptCreateDialog::_notification(int p_what) {

	switch (p_what) {
		case NOTIFICATION_THEME_CHANGED:
		case NOTIFICATION_ENTER_TREE: {
			path_button->set_icon(get_icon("Folder", "EditorIcons"));
			parent_browse_button->set_icon(get_icon("Folder", "EditorIcons"));
			parent_search_button->set_icon(get_icon("ClassList", "EditorIcons"));
			status_panel->add_style_override("panel", get_stylebox("bg", "Tree"));
		} break;
	}
}

void ScriptCreateDialog::_path_hbox_sorted() {
	if (is_visible()) {
		int filename_start_pos = initial_bp.find_last("/") + 1;
		int filename_end_pos = initial_bp.length();

		file_path->select(filename_start_pos, filename_end_pos);

		// First set cursor to the end of line to scroll LineEdit view
		// to the right and then set the actual cursor position.
		file_path->set_cursor_position(file_path->get_text().length());
		file_path->set_cursor_position(filename_start_pos);

		file_path->grab_focus();
	}
}

bool ScriptCreateDialog::_can_be_built_in() {
	return (supports_built_in && built_in_enabled);
}

void ScriptCreateDialog::config(const String &p_base_name, const String &p_base_path, bool p_built_in_enabled) {

	class_name->set_text("");
	class_name->deselect();
	parent_name->set_text(p_base_name);
	parent_name->deselect();

	if (p_base_path != "") {
		initial_bp = p_base_path.get_basename();
		file_path->set_text(initial_bp + "." + ScriptServer::get_language(language_menu->get_selected())->get_extension());
	} else {
		initial_bp = "";
		file_path->set_text("");
	}
	file_path->deselect();

	built_in_enabled = p_built_in_enabled;

	_lang_changed(current_language);
	_class_name_changed("");
	_path_changed(file_path->get_text());
}

void ScriptCreateDialog::set_inheritance_base_type(const String &p_base) {

	base_type = p_base;
}

bool ScriptCreateDialog::_validate_parent(const String &p_string) {

	if (p_string.length() == 0)
		return false;

	if (can_inherit_from_file && p_string.is_quoted()) {
		String p = p_string.substr(1, p_string.length() - 2);
		if (_validate_path(p, true) == "")
			return true;
	}

	return ClassDB::class_exists(p_string) || ScriptServer::is_global_class(p_string);
}

bool ScriptCreateDialog::_validate_class(const String &p_string) {

	if (p_string.length() == 0)
		return false;

	for (int i = 0; i < p_string.length(); i++) {

		if (i == 0) {
			if (p_string[0] >= '0' && p_string[0] <= '9')
				return false; // no start with number plz
		}

		bool valid_char = (p_string[i] >= '0' && p_string[i] <= '9') || (p_string[i] >= 'a' && p_string[i] <= 'z') || (p_string[i] >= 'A' && p_string[i] <= 'Z') || p_string[i] == '_' || p_string[i] == '.';

		if (!valid_char)
			return false;
	}

	return true;
}

String ScriptCreateDialog::_validate_path(const String &p_path, bool p_file_must_exist) {

	String p = p_path.strip_edges();

	if (p == "") return TTR("Path is empty.");
	if (p.get_file().get_basename() == "") return TTR("Filename is empty.");

	p = ProjectSettings::get_singleton()->localize_path(p);
	if (!p.begins_with("res://")) return TTR("Path is not local.");

	DirAccess *d = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (d->change_dir(p.get_base_dir()) != OK) {
		memdelete(d);
		return TTR("Invalid base path.");
	}
	memdelete(d);

	/* Does file already exist */
	DirAccess *f = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	if (f->dir_exists(p)) {
		memdelete(f);
		return TTR("A directory with the same name exists.");
	} else if (p_file_must_exist && !f->file_exists(p)) {
		memdelete(f);
		return TTR("File does not exist.");
	}
	memdelete(f);

	/* Check file extension */
	String extension = p.get_extension();
	List<String> extensions;

	// get all possible extensions for script
	for (int l = 0; l < language_menu->get_item_count(); l++) {
		ScriptServer::get_language(l)->get_recognized_extensions(&extensions);
	}

	bool found = false;
	bool match = false;
	int index = 0;
	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		if (E->get().nocasecmp_to(extension) == 0) {
			//FIXME (?) - changing language this way doesn't update controls, needs rework
			//language_menu->select(index); // change Language option by extension
			found = true;
			if (E->get() == ScriptServer::get_language(language_menu->get_selected())->get_extension()) {
				match = true;
			}
			break;
		}
		index++;
	}

	if (!found) return TTR("Invalid extension.");
	if (!match) return TTR("Wrong extension chosen.");

	/* Let ScriptLanguage do custom validation */
	String path_error = ScriptServer::get_language(language_menu->get_selected())->validate_path(p);
	if (path_error != "") return path_error;

	/* All checks passed */
	return "";
}

void ScriptCreateDialog::_class_name_changed(const String &p_name) {

	if (_validate_class(class_name->get_text())) {
		is_class_name_valid = true;
	} else {
		is_class_name_valid = false;
	}
	_update_dialog();
}

void ScriptCreateDialog::_parent_name_changed(const String &p_parent) {

	if (_validate_parent(parent_name->get_text())) {
		is_parent_name_valid = true;
	} else {
		is_parent_name_valid = false;
	}
	_update_dialog();
}

void ScriptCreateDialog::_template_changed(int p_template) {

	String selected_template = p_template == 0 ? "" : template_menu->get_item_text(template_menu->get_selected());
	EditorSettings::get_singleton()->set_project_metadata("script_setup", "last_selected_template", selected_template);
	if (p_template == 0) {
		//default
		script_template = "";
		return;
	}
	String ext = ScriptServer::get_language(language_menu->get_selected())->get_extension();
	String name = template_list[p_template - 1] + "." + ext;
	script_template = EditorSettings::get_singleton()->get_script_templates_dir().plus_file(name);
}

void ScriptCreateDialog::ok_pressed() {

	if (is_new_script_created) {
		_create_new();
	} else {
		_load_exist();
	}

	is_new_script_created = true;
	_update_dialog();
}

void ScriptCreateDialog::_create_new() {

	String cname_param;

	if (has_named_classes) {
		cname_param = class_name->get_text();
	} else {
		cname_param = ProjectSettings::get_singleton()->localize_path(file_path->get_text()).get_file().get_basename();
	}

	Ref<Script> scr;
	if (script_template != "") {
		scr = ResourceLoader::load(script_template);
		if (scr.is_null()) {
			alert->set_text(vformat(TTR("Error loading template '%s'"), script_template));
			alert->popup_centered();
			return;
		}
		scr = scr->duplicate();
		ScriptServer::get_language(language_menu->get_selected())->make_template(cname_param, parent_name->get_text(), scr);
	} else {
		scr = ScriptServer::get_language(language_menu->get_selected())->get_template(cname_param, parent_name->get_text());
	}

	if (has_named_classes) {
		String cname = class_name->get_text();
		if (cname.length())
			scr->set_name(cname);
	}

	if (!is_built_in) {
		String lpath = ProjectSettings::get_singleton()->localize_path(file_path->get_text());
		scr->set_path(lpath);
		Error err = ResourceSaver::save(lpath, scr, ResourceSaver::FLAG_CHANGE_PATH);
		if (err != OK) {
			alert->set_text(TTR("Error - Could not create script in filesystem."));
			alert->popup_centered();
			return;
		}
	}

	emit_signal("script_created", scr);
	hide();
}

void ScriptCreateDialog::_load_exist() {

	String path = file_path->get_text();
	RES p_script = ResourceLoader::load(path, "Script");
	if (p_script.is_null()) {
		alert->set_text(vformat(TTR("Error loading script from %s"), path));
		alert->popup_centered();
		return;
	}

	emit_signal("script_created", p_script.get_ref_ptr());
	hide();
}

void ScriptCreateDialog::_lang_changed(int l) {

	ScriptLanguage *language = ScriptServer::get_language(l);

	has_named_classes = language->has_named_classes();
	can_inherit_from_file = language->can_inherit_from_file();
	supports_built_in = language->supports_builtin_mode();
	if (!supports_built_in)
		is_built_in = false;

	String selected_ext = "." + language->get_extension();
	String path = file_path->get_text();
	String extension = "";
	if (path != "") {
		if (path.find(".") != -1) {
			extension = path.get_extension();
		}

		if (extension.length() == 0) {
			// add extension if none
			path += selected_ext;
			_path_changed(path);
		} else {
			// change extension by selected language
			List<String> extensions;
			// get all possible extensions for script
			for (int m = 0; m < language_menu->get_item_count(); m++) {
				ScriptServer::get_language(m)->get_recognized_extensions(&extensions);
			}

			for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
				if (E->get().nocasecmp_to(extension) == 0) {
					path = path.get_basename() + selected_ext;
					_path_changed(path);
					break;
				}
			}
		}
	} else {
		path = "class" + selected_ext;
		_path_changed(path);
	}
	file_path->set_text(path);

	bool use_templates = language->is_using_templates();
	template_menu->set_disabled(!use_templates);
	template_menu->clear();
	if (use_templates) {

		template_list = EditorSettings::get_singleton()->get_script_templates(language->get_extension());

		String last_lang = EditorSettings::get_singleton()->get_project_metadata("script_setup", "last_selected_language", "");
		String last_template = EditorSettings::get_singleton()->get_project_metadata("script_setup", "last_selected_template", "");

		template_menu->add_item(TTR("Default"));
		for (int i = 0; i < template_list.size(); i++) {
			String s = template_list[i].capitalize();
			template_menu->add_item(s);
			if (language_menu->get_item_text(language_menu->get_selected()) == last_lang && last_template == s) {
				template_menu->select(i + 1);
			}
		}
	} else {

		template_menu->add_item(TTR("N/A"));
		script_template = "";
	}

	_template_changed(template_menu->get_selected());
	EditorSettings::get_singleton()->set_project_metadata("script_setup", "last_selected_language", language_menu->get_item_text(language_menu->get_selected()));

	_parent_name_changed(parent_name->get_text());
	_update_dialog();
}

void ScriptCreateDialog::_built_in_pressed() {

	if (internal->is_pressed()) {
		is_built_in = true;
		is_new_script_created = true;
	} else {
		is_built_in = false;
		_path_changed(file_path->get_text());
	}
	_update_dialog();
}

void ScriptCreateDialog::_browse_path(bool browse_parent, bool p_save) {

	is_browsing_parent = browse_parent;

	if (p_save) {
		file_browse->set_mode(EditorFileDialog::MODE_SAVE_FILE);
		file_browse->set_title(TTR("Open Script / Choose Location"));
		file_browse->get_ok()->set_text(TTR("Open"));
	} else {
		file_browse->set_mode(EditorFileDialog::MODE_OPEN_FILE);
		file_browse->set_title(TTR("Open Script"));
	}

	file_browse->set_disable_overwrite_warning(true);
	file_browse->clear_filters();
	List<String> extensions;

	int lang = language_menu->get_selected();
	ScriptServer::get_language(lang)->get_recognized_extensions(&extensions);

	for (List<String>::Element *E = extensions.front(); E; E = E->next()) {
		file_browse->add_filter("*." + E->get());
	}

	file_browse->set_current_path(file_path->get_text());
	file_browse->popup_centered_ratio();
}

void ScriptCreateDialog::_file_selected(const String &p_file) {

	String p = ProjectSettings::get_singleton()->localize_path(p_file);
	if (is_browsing_parent) {
		parent_name->set_text("\"" + p + "\"");
		_parent_name_changed(parent_name->get_text());
	} else {
		file_path->set_text(p);
		_path_changed(p);

		String filename = p.get_file().get_basename();
		int select_start = p.find_last(filename);
		file_path->select(select_start, select_start + filename.length());
		file_path->set_cursor_position(select_start + filename.length());
		file_path->grab_focus();
	}
}

void ScriptCreateDialog::_create() {

	parent_name->set_text(select_class->get_selected_type().split(" ")[0]);
	_parent_name_changed(parent_name->get_text());
}

void ScriptCreateDialog::_browse_class_in_tree() {

	select_class->set_base_type(base_type);
	select_class->popup_create(true);
}

void ScriptCreateDialog::_path_changed(const String &p_path) {

	is_path_valid = false;
	is_new_script_created = true;

	String path_error = _validate_path(p_path, false);
	if (path_error != "") {
		_msg_path_valid(false, path_error);
		_update_dialog();
		return;
	}

	/* Does file already exist */
	DirAccess *f = DirAccess::create(DirAccess::ACCESS_RESOURCES);
	String p = ProjectSettings::get_singleton()->localize_path(p_path.strip_edges());
	if (f->file_exists(p)) {
		is_new_script_created = false;
		_msg_path_valid(true, TTR("File exists, it will be reused."));
	}
	memdelete(f);

	is_path_valid = true;
	_update_dialog();
}

void ScriptCreateDialog::_path_entered(const String &p_path) {
	ok_pressed();
}

void ScriptCreateDialog::_msg_script_valid(bool valid, const String &p_msg) {

	error_label->set_text(TTR(p_msg));
	if (valid) {
		error_label->add_color_override("font_color", get_color("success_color", "Editor"));
	} else {
		error_label->add_color_override("font_color", get_color("error_color", "Editor"));
	}
}

void ScriptCreateDialog::_msg_path_valid(bool valid, const String &p_msg) {

	path_error_label->set_text(TTR(p_msg));
	if (valid) {
		path_error_label->add_color_override("font_color", get_color("success_color", "Editor"));
	} else {
		path_error_label->add_color_override("font_color", get_color("error_color", "Editor"));
	}
}

void ScriptCreateDialog::_update_dialog() {

	bool script_ok = true;

	/* "Add Script Dialog" gui logic and script checks */

	// Is Script Valid (order from top to bottom)
	get_ok()->set_disabled(true);
	if (!is_built_in && !is_path_valid) {
		_msg_script_valid(false, TTR("Invalid path."));
		script_ok = false;
	}
	if (has_named_classes && (is_new_script_created && !is_class_name_valid)) {
		_msg_script_valid(false, TTR("Invalid class name."));
		script_ok = false;
	}
	if (!is_parent_name_valid && is_new_script_created) {
		_msg_script_valid(false, TTR("Invalid inherited parent name or path."));
		script_ok = false;
	}
	if (script_ok) {
		_msg_script_valid(true, TTR("Script is valid."));
		get_ok()->set_disabled(false);
	}

	/* Does script have named classes */

	if (has_named_classes) {
		if (is_new_script_created) {
			class_name->set_editable(true);
			class_name->set_placeholder(TTR("Allowed: a-z, A-Z, 0-9, _ and ."));
			class_name->set_placeholder_alpha(0.3);
		} else {
			class_name->set_editable(false);
		}
	} else {
		class_name->set_editable(false);
		class_name->set_placeholder(TTR("N/A"));
		class_name->set_placeholder_alpha(1);
		class_name->set_text("");
	}

	/* Is script Built-in */

	if (is_built_in) {
		file_path->set_editable(false);
		path_button->set_disabled(true);
		re_check_path = true;
	} else {
		file_path->set_editable(true);
		path_button->set_disabled(false);
		if (re_check_path) {
			re_check_path = false;
			_path_changed(file_path->get_text());
		}
	}

	/* Is Script created or loaded from existing file */

	if (is_built_in) {
		get_ok()->set_text(TTR("Create"));
		parent_name->set_editable(true);
		parent_search_button->set_disabled(false);
		parent_browse_button->set_disabled(!can_inherit_from_file);
		internal->set_visible(_can_be_built_in());
		internal_label->set_visible(_can_be_built_in());
		_msg_path_valid(true, TTR("Built-in script (into scene file)."));
	} else if (is_new_script_created) {
		// New Script Created
		get_ok()->set_text(TTR("Create"));
		parent_name->set_editable(true);
		parent_search_button->set_disabled(false);
		parent_browse_button->set_disabled(!can_inherit_from_file);
		internal->set_visible(_can_be_built_in());
		internal_label->set_visible(_can_be_built_in());
		if (is_path_valid) {
			_msg_path_valid(true, TTR("Will create a new script file."));
		}
	} else {
		// Script Loaded
		get_ok()->set_text(TTR("Load"));
		parent_name->set_editable(false);
		parent_search_button->set_disabled(true);
		parent_browse_button->set_disabled(true);
		internal->set_disabled(!_can_be_built_in());
		if (is_path_valid) {
			_msg_path_valid(true, TTR("Will load an existing script file."));
		}
	}
}

void ScriptCreateDialog::_bind_methods() {

	ClassDB::bind_method("_path_hbox_sorted", &ScriptCreateDialog::_path_hbox_sorted);
	ClassDB::bind_method("_class_name_changed", &ScriptCreateDialog::_class_name_changed);
	ClassDB::bind_method("_parent_name_changed", &ScriptCreateDialog::_parent_name_changed);
	ClassDB::bind_method("_lang_changed", &ScriptCreateDialog::_lang_changed);
	ClassDB::bind_method("_built_in_pressed", &ScriptCreateDialog::_built_in_pressed);
	ClassDB::bind_method("_browse_path", &ScriptCreateDialog::_browse_path);
	ClassDB::bind_method("_file_selected", &ScriptCreateDialog::_file_selected);
	ClassDB::bind_method("_path_changed", &ScriptCreateDialog::_path_changed);
	ClassDB::bind_method("_path_entered", &ScriptCreateDialog::_path_entered);
	ClassDB::bind_method("_template_changed", &ScriptCreateDialog::_template_changed);
	ClassDB::bind_method("_create", &ScriptCreateDialog::_create);
	ClassDB::bind_method("_browse_class_in_tree", &ScriptCreateDialog::_browse_class_in_tree);

	ClassDB::bind_method(D_METHOD("config", "inherits", "path", "built_in_enabled"), &ScriptCreateDialog::config, DEFVAL(true));

	ADD_SIGNAL(MethodInfo("script_created", PropertyInfo(Variant::OBJECT, "script", PROPERTY_HINT_RESOURCE_TYPE, "Script")));
}

ScriptCreateDialog::ScriptCreateDialog() {

	/* DIALOG */

	/* Main Controls */

	GridContainer *gc = memnew(GridContainer);
	gc->set_columns(2);

	/* Error Messages Field */

	VBoxContainer *vb = memnew(VBoxContainer);

	HBoxContainer *hb = memnew(HBoxContainer);
	Label *l = memnew(Label);
	l->set_text(" - ");
	hb->add_child(l);
	error_label = memnew(Label);
	error_label->set_text(TTR("Error!"));
	error_label->set_align(Label::ALIGN_LEFT);
	hb->add_child(error_label);
	vb->add_child(hb);

	hb = memnew(HBoxContainer);
	l = memnew(Label);
	l->set_text(" - ");
	hb->add_child(l);
	path_error_label = memnew(Label);
	path_error_label->set_text(TTR("Error!"));
	path_error_label->set_align(Label::ALIGN_LEFT);
	hb->add_child(path_error_label);
	vb->add_child(hb);

	status_panel = memnew(PanelContainer);
	status_panel->set_h_size_flags(Control::SIZE_FILL);
	status_panel->add_style_override("panel", EditorNode::get_singleton()->get_gui_base()->get_stylebox("bg", "Tree"));
	status_panel->add_child(vb);

	/* Spacing */

	Control *spacing = memnew(Control);
	spacing->set_custom_minimum_size(Size2(0, 10 * EDSCALE));

	vb = memnew(VBoxContainer);
	vb->add_child(gc);
	vb->add_child(spacing);
	vb->add_child(status_panel);
	hb = memnew(HBoxContainer);
	hb->add_child(vb);

	add_child(hb);

	/* Language */

	language_menu = memnew(OptionButton);
	language_menu->set_custom_minimum_size(Size2(250, 0) * EDSCALE);
	language_menu->set_h_size_flags(SIZE_EXPAND_FILL);
	l = memnew(Label(TTR("Language")));
	l->set_align(Label::ALIGN_RIGHT);
	gc->add_child(l);
	gc->add_child(language_menu);

	int default_lang = 0;
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {

		String lang = ScriptServer::get_language(i)->get_name();
		language_menu->add_item(lang);
		if (lang == "GDScript") {
			default_lang = i;
		}
	}

	String last_selected_language = EditorSettings::get_singleton()->get_project_metadata("script_setup", "last_selected_language", "");
	if (last_selected_language != "") {
		for (int i = 0; i < language_menu->get_item_count(); i++) {
			if (language_menu->get_item_text(i) == last_selected_language) {
				language_menu->select(i);
				current_language = i;
				break;
			}
		}
	} else {
		language_menu->select(default_lang);
		current_language = default_lang;
	}

	language_menu->connect("item_selected", this, "_lang_changed");

	/* Inherits */

	base_type = "Object";

	hb = memnew(HBoxContainer);
	hb->set_h_size_flags(SIZE_EXPAND_FILL);
	parent_name = memnew(LineEdit);
	parent_name->connect("text_changed", this, "_parent_name_changed");
	parent_name->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(parent_name);
	parent_search_button = memnew(Button);
	parent_search_button->set_flat(true);
	parent_search_button->connect("pressed", this, "_browse_class_in_tree");
	hb->add_child(parent_search_button);
	parent_browse_button = memnew(Button);
	parent_browse_button->set_flat(true);
	parent_browse_button->connect("pressed", this, "_browse_path", varray(true, false));
	hb->add_child(parent_browse_button);
	l = memnew(Label(TTR("Inherits")));
	l->set_align(Label::ALIGN_RIGHT);
	gc->add_child(l);
	gc->add_child(hb);
	is_browsing_parent = false;

	/* Class Name */

	class_name = memnew(LineEdit);
	class_name->connect("text_changed", this, "_class_name_changed");
	class_name->set_h_size_flags(SIZE_EXPAND_FILL);
	l = memnew(Label(TTR("Class Name")));
	l->set_align(Label::ALIGN_RIGHT);
	gc->add_child(l);
	gc->add_child(class_name);

	/* Templates */

	template_menu = memnew(OptionButton);
	l = memnew(Label(TTR("Template")));
	l->set_align(Label::ALIGN_RIGHT);
	gc->add_child(l);
	gc->add_child(template_menu);
	template_menu->connect("item_selected", this, "_template_changed");

	/* Built-in Script */

	internal = memnew(CheckBox);
	internal->set_text(TTR("On"));
	internal->connect("pressed", this, "_built_in_pressed");
	internal_label = memnew(Label(TTR("Built-in Script")));
	internal_label->set_align(Label::ALIGN_RIGHT);
	gc->add_child(internal_label);
	gc->add_child(internal);

	/* Path */

	hb = memnew(HBoxContainer);
	hb->connect("sort_children", this, "_path_hbox_sorted");
	file_path = memnew(LineEdit);
	file_path->connect("text_changed", this, "_path_changed");
	file_path->connect("text_entered", this, "_path_entered");
	file_path->set_h_size_flags(SIZE_EXPAND_FILL);
	hb->add_child(file_path);
	path_button = memnew(Button);
	path_button->set_flat(true);
	path_button->connect("pressed", this, "_browse_path", varray(false, true));
	hb->add_child(path_button);
	l = memnew(Label(TTR("Path")));
	l->set_align(Label::ALIGN_RIGHT);
	gc->add_child(l);
	gc->add_child(hb);

	/* Dialog Setup */

	select_class = memnew(CreateDialog);
	select_class->connect("create", this, "_create");
	add_child(select_class);

	file_browse = memnew(EditorFileDialog);
	file_browse->connect("file_selected", this, "_file_selected");
	file_browse->set_mode(EditorFileDialog::MODE_OPEN_FILE);
	add_child(file_browse);
	get_ok()->set_text(TTR("Create"));
	alert = memnew(AcceptDialog);
	alert->set_as_minsize();
	alert->get_label()->set_autowrap(true);
	alert->get_label()->set_align(Label::ALIGN_CENTER);
	alert->get_label()->set_valign(Label::VALIGN_CENTER);
	alert->get_label()->set_custom_minimum_size(Size2(325, 60) * EDSCALE);
	add_child(alert);

	set_as_minsize();
	set_hide_on_ok(false);
	set_title(TTR("Attach Node Script"));

	is_parent_name_valid = false;
	is_class_name_valid = false;
	is_path_valid = false;

	has_named_classes = false;
	supports_built_in = false;
	can_inherit_from_file = false;
	built_in_enabled = true;
	is_built_in = false;

	is_new_script_created = true;
}
