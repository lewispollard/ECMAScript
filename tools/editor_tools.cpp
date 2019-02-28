#include "editor_tools.h"
#include "../ecmascript_language.h"
#include "../ecmascript_library.h"
#include "editor/filesystem_dock.h"

struct ECMAScriptAlphCompare {
	_FORCE_INLINE_ bool operator()(const Ref<ECMAScript> &l, const Ref<ECMAScript> &r) const {
		return String(l->get_class_name()) < String(r->get_class_name());
	}
};

void ECMAScriptPlugin::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_bottom_panel_toggled"), &ECMAScriptPlugin::_on_bottom_panel_toggled);
	ClassDB::bind_method(D_METHOD("_on_menu_item_pressed"), &ECMAScriptPlugin::_on_menu_item_pressed);
	ClassDB::bind_method(D_METHOD("_export_typescript_declare_file"), &ECMAScriptPlugin::_export_typescript_declare_file);
}

void ECMAScriptPlugin::_on_bottom_panel_toggled(bool pressed) {
	if (pressed) {
		ecma_class_browser->update_tree();
	}
}

void ECMAScriptPlugin::_on_menu_item_pressed(int item) {
	switch (item) {
		case MenuItem::ITEM_RELOAD_LIBS:
			ecma_class_browser->reload_cached_libs();
			break;
		case MenuItem::ITEM_GEN_DECLAR_FILE:
			declaration_file_dialog->popup_centered_ratio();
			break;
	}
}

void ECMAClassBrower::_on_filter_changed(const String &p_text) {
	update_tree();
}

Variant ECMAClassBrower::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (p_from == class_tree) {
		TreeItem *item = class_tree->get_item_at_position(class_tree->get_local_mouse_position());
		if (item) {
			Ref<ECMAScript> script = item->get_metadata(0);
			Vector<String> paths;

			String class_dir = String("res://ECMAClass/");
			String path = class_dir + script->get_class_name() + ".es";
			if (!res_dir->dir_exists(class_dir)) {
				res_dir->make_dir_recursive(class_dir);
			}
			if (!res_dir->file_exists(path)) {
				ResourceSaver::save(path, script);
			}

			paths.push_back(path);
			Dictionary drag_data = EditorNode::get_singleton()->drag_files_and_dirs(paths, p_from);
			return drag_data;
		}
	}
	return NULL;
}

ECMAScriptPlugin::ECMAScriptPlugin(EditorNode *p_node) {
	ecma_class_browser = memnew(ECMAClassBrower);
	bottom_button = p_node->add_bottom_panel_item("ECMAScript", ecma_class_browser);
	bottom_button->connect("toggled", this, "_on_bottom_panel_toggled");

	eslib_inspector_plugin.instance();
	EditorInspector::add_inspector_plugin(eslib_inspector_plugin);

	PopupMenu *menu = memnew(PopupMenu);
	add_tool_submenu_item(TTR("ECMAScript"), menu);
	menu->add_item(TTR("Reload All Cached Libraries"), ITEM_RELOAD_LIBS);
	menu->add_item(TTR("Generate TypeScript Declaration File"), ITEM_GEN_DECLAR_FILE);
	menu->connect("id_pressed", this, "_on_menu_item_pressed");

	declaration_file_dialog = memnew(EditorFileDialog);
	declaration_file_dialog->set_title(TTR("Generate TypeScript Declaration File"));
	declaration_file_dialog->set_mode(EditorFileDialog::MODE_SAVE_FILE);
	declaration_file_dialog->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	declaration_file_dialog->add_filter(TTR("*.d.ts;TypeScript Declaration file"));
	declaration_file_dialog->set_current_file("godot.d.ts");
	declaration_file_dialog->connect("file_selected", this, "_export_typescript_declare_file");
	EditorNode::get_singleton()->get_gui_base()->add_child(declaration_file_dialog);
}

void ECMAClassBrower::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_on_filter_changed"), &ECMAClassBrower::_on_filter_changed);
	ClassDB::bind_method(D_METHOD("get_drag_data_fw"), &ECMAClassBrower::get_drag_data_fw);
	ClassDB::bind_method(D_METHOD("reload_cached_libs"), &ECMAClassBrower::reload_cached_libs);
}

void ECMAClassBrower::update_tree() {

	String filter = filter_input->get_text();
	class_tree->clear();

	List<Ref<ECMAScript> > classes;
	ECMAScriptLanguage::get_singleton()->get_registered_classes(classes);
	classes.sort_custom<ECMAScriptAlphCompare>();

	Ref<Texture> script_icon = get_icon("Script", "EditorIcons");

	TreeItem *root = class_tree->create_item();
	for (List<Ref<ECMAScript> >::Element *E = classes.front(); E; E = E->next()) {

		const Ref<ECMAScript> &script = E->get();
		if (!script->is_valid()) {
			continue;
		}

		String lib_path;
		const Ref<ECMAScriptLibrary> &lib = script->get_library();
		if (lib.is_valid()) {
			lib_path = lib->get_path();
		}

		String name = script->get_class_name();
		String native_class_name = script->get_ecma_class()->native_class->name;
		if (filter.empty() || filter.is_subsequence_ofi(name) || filter.is_subsequence_ofi(native_class_name)) {
			TreeItem *item = class_tree->create_item(root);
			item->set_metadata(0, script);
			item->set_metadata(1, script);
			item->set_metadata(2, script);
			item->set_text(0, name);
			item->set_text(1, native_class_name);

			if (script->get_library().is_valid()) {
				lib_path = script->get_library()->get_path();
			}
			item->set_text(2, lib_path);
			item->set_icon(0, script_icon);
			item->set_text_align(0, TreeItem::ALIGN_LEFT);
			item->set_text_align(1, TreeItem::ALIGN_CENTER);
			item->set_text_align(2, TreeItem::ALIGN_CENTER);
		}
	}
}

void ECMAClassBrower::reload_cached_libs() {
	ECMAScriptLibraryResourceLoader::reload_cached_libs();
	update_tree();
}

ECMAClassBrower::ECMAClassBrower() :
		res_dir(DirAccess::open("res://")) {

	set_custom_minimum_size(Size2(0, 300));

	HBoxContainer *hbox_top = memnew(HBoxContainer);
	Label *title = memnew(Label);
	title->set_text(TTR("Classes registered in ECMAScript"));
	title->set_h_size_flags(Control::SIZE_EXPAND);
	hbox_top->add_child(title);
	Button *bt_reload = memnew(Button);
	bt_reload->set_tooltip(TTR("Reload all cached libaries"));
	bt_reload->connect("pressed", this, "reload_cached_libs");
	//	bt_reload->set_icon(get_icon("Reload", "EditorIcons"));
	bt_reload->set_text(TTR("Reload"));
	hbox_top->add_child(bt_reload);
	add_child(hbox_top);

	class_tree = memnew(Tree);
	class_tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	class_tree->set_hide_root(true);
	class_tree->set_column_titles_visible(true);
	class_tree->set_select_mode(Tree::SELECT_ROW);
	class_tree->set_columns(3);
	class_tree->set_column_title(0, TTR("Script Class"));
	class_tree->set_column_title(1, TTR("Native Class"));
	class_tree->set_column_title(2, TTR("Library"));
	class_tree->set_drag_forwarding(this);
	add_child(class_tree);

	HBoxContainer *hbox = memnew(HBoxContainer);
	Label *filter_text = memnew(Label);
	filter_text->set_text(TTR("Filter:"));
	filter_input = memnew(LineEdit);
	hbox->add_child(filter_text);
	hbox->add_child(filter_input);
	filter_input->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	filter_input->connect("text_changed", this, "_on_filter_changed");
	add_child(hbox);
}

void EditorInspectorPluginECMALib::_bind_methods() {
	ClassDB::bind_method(D_METHOD("on_reload_editing_lib"), &EditorInspectorPluginECMALib::on_reload_editing_lib);
}

void EditorInspectorPluginECMALib::on_reload_editing_lib() {
	ERR_FAIL_NULL(editing_lib);
	editing_lib->reload_from_file();
}

bool EditorInspectorPluginECMALib::can_handle(Object *p_object) {
	return Object::cast_to<ECMAScriptLibrary>(p_object) != NULL;
}

void EditorInspectorPluginECMALib::parse_begin(Object *p_object) {
	editing_lib = Object::cast_to<ECMAScriptLibrary>(p_object);
	ERR_FAIL_COND(!editing_lib);

	Button *button = memnew(Button);
	button->set_text("Reload");
	button->connect("pressed", this, "on_reload_editing_lib");
	add_custom_control(button);
}

EditorInspectorPluginECMALib::EditorInspectorPluginECMALib() {
	editing_lib = NULL;
}


static String applay_partern(const String & p_partern, const Dictionary &p_values) {
	String ret = p_partern;
	for (const Variant* key = p_values.next(); key; key = p_values.next(key)) {
		String p = String("${") + String(*key) + "}";
		String v = p_values.get(*key, p);
		ret = ret.replace(p, v);
	}
	return ret;
}

static String format_doc_text(const String &p_source, const String & p_indent = "\t") {
	Dictionary dict;
	dict["[code]"] = "`";
	dict["[/code]"] = "`";
	dict["[codeblock]"] = "```gdscript";
	dict["[/codeblock]"] = "```";

	String ret = "";
	Vector<String> lines = p_source.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		String line = lines[i].strip_edges();
		if (line.strip_edges().empty()) continue;
		ret += p_indent + lines[i].strip_edges() + "  \n";
	}

	for (const Variant* key = dict.next(); key; key = dict.next(key)) {
		String p = *key;
		String v = dict.get(*key, p);
		ret = ret.replace(p, v);
	}
	return ret;
}

static String get_type_name(const String &p_type) {
	if (p_type.empty())
		return "void";
	if (p_type == "int" || p_type == "float")
		return "number";
	if (p_type == "bool")
		return "boolean";
	if (p_type == "String")
		return "string";
	return p_type;
}

String _export_method(const DocData::MethodDoc &p_method) {
	const String method_template = R"(
		/**
${description}
		*/
		${name}(${params}) : ${return_type};
)";
	Dictionary dict;
	dict["description"] = format_doc_text(p_method.description, "\t\t ");
	dict["name"] = p_method.name;
	dict["return_type"] = get_type_name(p_method.return_type);
	String params = "";
	for (int i=0; i<p_method.arguments.size(); i++) {
		const DocData::ArgumentDoc& arg = p_method.arguments[i];
		String arg_str = arg.name + ": " + get_type_name(arg.type);
		if (!arg.default_value.empty()) {
			arg_str += String(" = ") + arg.default_value;
		}
		if (i<p_method.arguments.size() -1) {
			arg_str += ", ";
		}
		params += arg_str;
	}
	dict["params"] = params;
	return applay_partern(method_template, dict);;
}

String _export_class(const DocData::ClassDoc &class_doc) {
	const String class_template = R"(
	/**
${brief_description}

${description}
	*/
	class ${name}${extends}${inherits} {
${constants}
${properties}
${methods}
	}
)";

	Dictionary dict;
	dict["name"] = class_doc.name;
	dict["inherits"] = class_doc.inherits;
	dict["extends"] = class_doc.inherits.empty() ? "" : " extends ";
	String brief_description = format_doc_text(class_doc.brief_description, "\t ");
	dict["brief_description"] = brief_description;
	String description = format_doc_text(class_doc.description, "\t ");
	if (description.length() == brief_description.length() && description == brief_description) {
		dict["description"] = "";
	} else {
		dict["description"] = description;
	}

	String constants = "";
	for (int i=0; i<class_doc.constants.size(); i++) {
		const DocData::ConstantDoc & const_doc = class_doc.constants[i];
		String const_str = R"(
		/**
${description}
		*/
		static readonly ${name}: number = ${value};
)";
		Dictionary dict;
		dict["description"] = format_doc_text(const_doc.description, "\t\t ");
		dict["name"] = const_doc.name;
		dict["value"] = const_doc.value;
		constants += applay_partern(const_str, dict);;
	}
	dict["constants"] = constants;


	String properties = "";
	for (int i=0; i<class_doc.properties.size(); i++) {
		const DocData::PropertyDoc & prop_doc = class_doc.properties[i];
		String prop_str = R"(
		/**
${description}
		*/
		${name}: ${type};
)";
		Dictionary dict;
		dict["description"] = format_doc_text(prop_doc.description, "\t\t ");
		dict["name"] = prop_doc.name;
		dict["type"] = get_type_name(prop_doc.type);
		properties += applay_partern(prop_str, dict);;
	}
	dict["properties"] = properties;

	String methods = "";
	for (int i=0; i<class_doc.methods.size(); i++) {
		const DocData::MethodDoc & method_doc = class_doc.methods[i];
		methods += _export_method(method_doc);
	}
	dict["methods"] = methods;

	return applay_partern(class_template, dict);
}


void ECMAScriptPlugin::_export_typescript_declare_file(const String &p_path) {
	DocData *doc = EditorHelp::get_doc_data();

	const String godot_module = R"(
declare module godot {
	${classes}
})";
	String classes = "";
	Set<String> ignored_classes;
	ignored_classes.insert("int");
	ignored_classes.insert("float");
	ignored_classes.insert("bool");
	ignored_classes.insert("Array");
	ignored_classes.insert("Nil");

	for (Map<String, DocData::ClassDoc>::Element *E = doc->class_list.front(); E; E = E->next()) {
		const DocData::ClassDoc & class_doc = E->get();
		if (class_doc.name.begins_with("@") || ignored_classes.has(class_doc.name)) {
			continue;
		}
		classes += _export_class(class_doc);
	}

	Dictionary dict;
	dict["classes"] = classes;
	String text = applay_partern(godot_module, dict);

	FileAccessRef f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.f && f->is_open()) {
		f->store_string(text);
		f->close();
	}
}
