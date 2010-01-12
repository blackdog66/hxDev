/*
 * Copyright the original author or authors.
 *
 * Licensed under the MOZILLA PUBLIC LICENSE, Version 1.1 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.mozilla.org/MPL/MPL-1.1.html
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Initial author:
 *      Ritchie Turner (a.k.a. blackdog)
 *              iPowerHouse.com
 *
 * Additional code by:
 *      Danny Wilson (a.k.a. BlueZeniX)
 *              deCube.net - design and development
 */
#include <string.h>
#include "geany.h"
#include "support.h"
#include "build.h"
#include "plugindata.h"
#include "document.h"
#include "neko_vm.h"
#include "geanyobject.h"
#include "project.h"
#include "SciLexer.h"
#include "Scintilla.h"
#include "filetypes.h"


typedef struct SCNotification SCNotification;
#define SSM(s, m, w, l) geany->sci->send_message(s, m, w, l)

// Neko should only need to resolve these once
field ID_completeDot;
field ID_completeBracket;
field ID_checkLine;

value hxPackageSearch();
value hxNekoLoad(char *);
value hxSetHaxeLibraryPath(value);
value hxPluginDir();
value hxDocumentCreate(value,value);
value hxDocumentOpen(value name);
value hxDocumentSearch(value,value);
value hxDocumentCurrent();
value hxDocumentIndex();
value hxDocumentRemove(value ind);
value hxDocumentSetText(value gDoc,value text) ;
value hxDocumentGetSelectedLength(value gDoc);
value hxDocumentGetSelectedText(value gDoc);
value hxDocumentGetText(value gDoc);
value hxDocumentGetLength(value gDoc);
value hxGetCurrentPosition();
value hxDocumentInsertText(value gDoc,value pos,value text);
value hxDocumentGetLineFromPosition(value gDoc,value pos);
value hxDocumentGetLine(value gDoc,value line);
value hxDocumentGetCharAt(value gDoc,value pos);
value hxDocumentPosition(value gDoc);
value hxDocumentGetColFromPosition(value gDoc,value pos);
value hxDocumentReplaceSelection(value gDoc,value text) ;
value hxDocumentSetSelectionStart(value gDoc,value pos);
value hxDocumentSetSelectionEnd(value gDoc,value pos);
value hxDocumentGetSelectionStart(value gDoc);
value hxDocumentGetSelectionEnd(value gDoc);

void hxCheckPlatform(ScintillaObject *sci,value extras,int start) ;
value hxCompletion(ScintillaObject *sci,char *name,char trigger,int idx);
void hxCallTip(ScintillaObject *sci,guint pos,char *str);
value hxProjectDesc() ;
value hxProjectBase() ;
value hxStatusAdd(value) ;
static value hxGetExtrasInstance();
static value docOnClassPath;
value hxCompilerAdd(value msg);

void check(value v,field f,void *p) ;
static void on_activate(GObject *obj, gint idx, gpointer user_data);

DEFINE_KIND(k_hxDoc);

GeanyData *geany;
#define doc_array       geany->doc_array

static struct {
	GtkWidget *menu_item;
	neko_vm *vm;
	char *HAXE_LIBRARY_PATH;
	char *plugin_dir;
	char *module;
	value extrasModule;
	gint idx;
	value extras;
	char *projectBase;
//	value pluginReady;
} hxLocalData;

/* Check that Geany supports plugin API version 2 or later, and check
 * for binary compatibility. */
VERSION_CHECK(20)

/* All plugins must set name and description */
PLUGIN_INFO(_("Neko"), _("Neko VM Plugin."),"0.1")

/* Callback when the menu item is clicked */
static void
item_activate(GtkMenuItem *menuitem, gpointer gdata) {
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new ("Run Module",
						  GTK_WIDGET(geany->app->window),
						  GTK_FILE_CHOOSER_ACTION_OPEN,
						  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						  GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
						  NULL);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT)  {
		char *m = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		value extras = hxGetExtrasInstance();
		val_ocall1(extras,val_id("runModule"),alloc_string(m));
		g_free(m);
  	}

  	gtk_widget_destroy (dialog);
}

/*
 * Geany Callbacks first
 */

void init(GeanyData *data) {
	GtkWidget *demo_item;

	geany = data;	// keep a pointer to the main application fields & functions

	// Add an item to the Tools menu
	demo_item = gtk_menu_item_new_with_mnemonic(_("_Run Neko .n"));
	gtk_widget_show(demo_item);
	gtk_container_add(GTK_CONTAINER(geany->tools_menu), demo_item);
	g_signal_connect(G_OBJECT(demo_item), "activate", G_CALLBACK(item_activate), NULL);

	// keep a pointer to the menu item, so we can remove it when the plugin is unloaded
	hxLocalData.menu_item = demo_item;

    neko_global_init(NULL);
    hxLocalData.vm = neko_vm_alloc(NULL);

	// Prevent events to fire before the Neko plugin is ready
//	hxLocalData.isReady = alloc_bool(FALSE);
//	hxLocalData.isReady = val_false;


    hxLocalData.plugin_dir =
		g_strconcat(geany->app->configdir, G_DIR_SEPARATOR_S,
			 "plugins", G_DIR_SEPARATOR_S,
	 		 "neko",NULL);

	char *extras =
		g_strconcat(hxLocalData.plugin_dir, G_DIR_SEPARATOR_S,
	 		 "Extras.n",
	 		  NULL);

	neko_vm_select(hxLocalData.vm);
	hxLocalData.extrasModule = hxNekoLoad(extras);

	g_free(extras);
 }

 void cleanup() {
	gtk_widget_destroy(hxLocalData.menu_item);
	g_free(hxLocalData.plugin_dir);
	neko_global_free();
}

void on_sci_notify(GtkWidget *editor, gint scn, gpointer lscn, gpointer user_data) {
	SCNotification *nt;
	ScintillaObject *sci;
	gint idx;
	gchar *tmp;
	char *filename;

	idx = GPOINTER_TO_INT(user_data);
	sci = doc_list[idx].sci;
	nt = lscn;
	switch (nt->nmhdr.code) {
		case SCN_CHARADDED:
			switch (nt->ch) {
				case '(': case '.': /* case ';': */
					filename = DOC_FILENAME(idx);

					// Make the dot autocomplete
					if(nt->ch=='.' && SSM(sci, SCI_AUTOCACTIVE, NULL, NULL) != 0)
					{
						SSM(sci, SCI_AUTOCCOMPLETE, NULL, NULL);
						tmp = g_new(char,2); tmp[0]=nt->ch; tmp[1]='\0';
						SSM(sci, SCI_ADDTEXT, 1, (sptr_t)tmp);
						g_free(tmp);
						//nt->position++;
						nt->position = geany->sci->get_current_position(sci)+1;
						if( SSM(sci, SCI_GETCHARAT, nt->position-1, NULL) != nt->ch )
							on_sci_notify(editor,scn,nt,user_data);
					}
					else if(docOnClassPath == val_true) {
						hxCompletion(sci,filename,nt->ch,idx);
					}
					else printf("%s not on classpath\n",filename);
				break;
			}
		break;

		case SCN_USERLISTSELECTION:	break;
		case SCN_AUTOCSELECTION: 	break;
	}
}

static void on_doc(GObject *obj, gint idx, gpointer user_data) {
	ScintillaObject *sci;

	if (FILETYPE_ID(doc_list[idx].file_type) == GEANY_FILETYPES_HAXE) { // only do for haxe files
		gint id = GPOINTER_TO_INT(user_data);
		switch(id) 	{
			case GCB_DOCUMENT_NEW: case GCB_DOCUMENT_OPEN:
				sci = (ScintillaObject*)doc_list[idx].sci;
				SSM(sci, SCI_AUTOCSETCANCELATSTART, FALSE, NULL);
				SSM(sci, SCI_AUTOCSETAUTOHIDE, FALSE, NULL);
				SSM(sci, SCI_AUTOCSETIGNORECASE, TRUE, NULL);

				g_signal_connect((GtkWidget*) sci, "sci-notify",
					G_CALLBACK(on_sci_notify), GINT_TO_POINTER(idx));

				hxLocalData.idx = idx;
				value extras = hxGetExtrasInstance();
				docOnClassPath = val_ocall2(extras,val_id("changeDocument"),alloc_string(DOC_FILENAME(idx)),alloc_int(idx));

			break;
			case GCB_DOCUMENT_SAVE:	break;
		}
	}
}

GeanyCallback	geany_callbacks[] = {
	{"document-new", (GCallback) &on_doc, FALSE, GINT_TO_POINTER(GCB_DOCUMENT_NEW)},
	{"document-open", (GCallback) &on_doc, FALSE, GINT_TO_POINTER(GCB_DOCUMENT_OPEN)},
	{"document-save", (GCallback) &on_doc, FALSE, GINT_TO_POINTER(GCB_DOCUMENT_SAVE)},
	{"document-activate", (GCallback) &on_activate, FALSE, GINT_TO_POINTER(GCB_DOCUMENT_ACTIVATE)},
	{NULL, NULL, FALSE, NULL}
};

/*
 * Plugin Setup
 */

value hxNekoLoad( char *file ) {
    value loader;
    value apiObj = alloc_object(NULL);
    value args[2];
    value exc = NULL;
    value module;

    loader = neko_default_loader(NULL,0);
    alloc_field(loader,val_id("apiObj"),apiObj);

	alloc_field(apiObj,val_id("hxSetHaxeLibraryPath"),alloc_function(hxSetHaxeLibraryPath,1,"hxSetHaxeLibraryPath"));
	alloc_field(apiObj,val_id("hxPluginDir"),alloc_function(hxPluginDir,0,"hxPluginDir"));
	alloc_field(apiObj,val_id("hxProjectDesc"),alloc_function(hxProjectDesc,0,"hxProjectDesc"));
	alloc_field(apiObj,val_id("hxProjectBase"),alloc_function(hxProjectBase,0,"hxProjectBase"));
	alloc_field(apiObj,val_id("hxPackageSearch"),alloc_function(hxPackageSearch,0,"hxPackageSearch"));

	alloc_field(apiObj,val_id("hxDocumentCreate"),alloc_function(hxDocumentCreate,2,"hxDocumentCreate"));
	alloc_field(apiObj,val_id("hxDocumentOpen"),alloc_function(hxDocumentOpen,1,"hxDocumentOpen"));
	alloc_field(apiObj,val_id("hxDocumentSearch"),alloc_function(hxDocumentSearch,2,"hxDocumentSearch"));
	alloc_field(apiObj,val_id("hxDocumentIndex"),alloc_function(hxDocumentIndex,0,"hxDocumentIndex"));
	alloc_field(apiObj,val_id("hxDocumentRemove"),alloc_function(hxDocumentRemove,1,"hxDocumentRemove"));
	alloc_field(apiObj,val_id("hxDocumentCurrent"),alloc_function(hxDocumentCurrent,0,"hxDocumentCurrent"));
	alloc_field(apiObj,val_id("hxDocumentSetText"),alloc_function(hxDocumentSetText,2,"hxDocumentSetText"));
	alloc_field(apiObj,val_id("hxDocumentInsertText"),alloc_function(hxDocumentInsertText,3,"hxDocumentInsertText"));
	alloc_field(apiObj,val_id("hxDocumentPosition"),alloc_function(hxDocumentPosition,1,"hxDocumentPosition"));
	alloc_field(apiObj,val_id("hxDocumentGetText"),alloc_function(hxDocumentGetText,1,"hxDocumentGetText"));
	alloc_field(apiObj,val_id("hxDocumentGetLength"),alloc_function(hxDocumentGetLength,1,"hxDocumentGetLength"));
	alloc_field(apiObj,val_id("hxDocumentGetSelectedText"),alloc_function(hxDocumentGetSelectedText,1,"hxDocumentGetSelectedText"));
	alloc_field(apiObj,val_id("hxDocumentGetSelectedLength"),alloc_function(hxDocumentGetSelectedLength,1,"hxDocumentGetSelectedLength"));
	alloc_field(apiObj,val_id("hxDocumentGetLineFromPosition"),alloc_function(hxDocumentGetLineFromPosition,2,"hxDocumentGetLineFromPosition"));
	alloc_field(apiObj,val_id("hxDocumentGetLine"),alloc_function(hxDocumentGetLine,2,"hxDocumentGetLine"));
	alloc_field(apiObj,val_id("hxDocumentGetCharAt"),alloc_function(hxDocumentGetCharAt,2,"hxDocumentGetCharAt"));
	alloc_field(apiObj,val_id("hxDocumentGetColFromPosition"),alloc_function(hxDocumentGetColFromPosition,2,"hxDocumentGetColFromPosition"));
	alloc_field(apiObj,val_id("hxDocumentReplaceSelection"),alloc_function(hxDocumentReplaceSelection,2,"hxDocumentReplaceSelection"));
	alloc_field(apiObj,val_id("hxDocumentGetSelectionStart"),alloc_function(hxDocumentGetSelectionStart,1,"hxDocumentGetSelectionStart"));
	alloc_field(apiObj,val_id("hxDocumentGetSelectionEnd"),alloc_function(hxDocumentGetSelectionEnd,1,"hxDocumentGetSelectionEnd"));
	alloc_field(apiObj,val_id("hxDocumentSetSelectionStart"),alloc_function(hxDocumentSetSelectionStart,2,"hxDocumentSetSelectionStart"));
	alloc_field(apiObj,val_id("hxDocumentSetSelectionEnd"),alloc_function(hxDocumentSetSelectionEnd,2,"hxDocumentSetSelectionEnd"));

	alloc_field(apiObj,val_id("hxCompilerAdd"),alloc_function(hxCompilerAdd,1,"hxCompilerAdd"));
	alloc_field(apiObj,val_id("hxStatusAdd"),alloc_function(hxStatusAdd,1,"hxStatusAdd"));

	// Small optimization (couldn't resist :D)
	ID_completeDot = val_id("completeDot");
	ID_completeBracket = val_id("completeBracket");
	ID_checkLine = val_id("checkLine");

    args[0] = alloc_string(file);
    args[1] = loader;

    module = val_callEx(loader,val_field(loader,val_id("loadmodule")),args,2,&exc);
    if( exc != NULL ) {
        buffer b = alloc_buffer(NULL);
        val_buffer(b,exc);
        printf("Uncaught exception - %s\n",val_string(buffer_to_string(b)));
        return NULL;
    }
    return module;
}

static value hxGetExtrasInstance() {
	neko_vm_select(hxLocalData.vm);
	if (hxLocalData.extras == NULL) {
		value classes = val_field(hxLocalData.extrasModule,val_id("__classes")) ;
		value hxdev = val_field(classes,val_id("Extras")) ;
		hxLocalData.extras = val_field(hxdev,val_id("extras"));
	}
	return hxLocalData.extras ;
}

static void on_activate(GObject *obj, gint idx, gpointer user_data) {
	char *filename = DOC_FILENAME(idx);
	value extras = hxGetExtrasInstance();
	hxLocalData.idx = idx;
	docOnClassPath = val_ocall2(extras,val_id("changeDocument"),alloc_string(filename),alloc_int(idx));
}

value hxCompletion(ScintillaObject *sci,char *filename,char trigger,int idx) {

	value extras = hxGetExtrasInstance();
	value r;

	if (extras != val_null) {
		guint p = geany->sci->get_current_position(sci);

		// don't autocomplete in comments and strings
		gint style;
		style = SSM(sci, SCI_GETSTYLEAT, p, 0);
		switch(style)
		{
			case SCE_C_COMMENT:
			case SCE_C_COMMENTLINE:
			case SCE_C_COMMENTDOC:
			case SCE_C_COMMENTLINEDOC:
			case SCE_C_CHARACTER:
			case SCE_C_PREPROCESSOR:
			case SCE_C_STRING:
			return val_false;
		}

		value pos = alloc_int(p);
		value line = alloc_int(geany->sci->get_line_from_position(sci,p));

		hxCheckPlatform(sci,extras,p) ;

		char *syntaxError = NULL;
		char *str;

		switch(trigger)
		{
		  case '.':
			r = val_ocall2(extras,ID_completeDot,pos,line);
			str = val_string(r);
			if (*str) {
				if (strncmp(str,"SYN:",4) == 0){
					if (*(str+4) != '\0') syntaxError = str;
				} else {
					geany->sci->send_message(sci, SCI_AUTOCSHOW,0, (sptr_t)str);
				}
			} break;

		  case '(':
			r = val_ocall2(extras,ID_completeBracket,pos,line);
			str = val_string(r);

			if (*str) {
				if (strncmp(str,"SYN:",4) == 0) {
					if (*(str+4) != '\0') syntaxError = str;
				} else {
					hxCallTip(sci,p,str);
				}
			}

			break;
		}

		if (syntaxError != NULL) {
			hxCallTip(sci,p,syntaxError+4); // +4 to skip SYN:
		}
	}

    return val_true;
}

void hxCallTip(ScintillaObject *sci,guint pos,char *str) {
	if (!SSM(sci, SCI_CALLTIPACTIVE, 0, 0))
		SSM(sci, SCI_CALLTIPSHOW, pos, (sptr_t) str);
}

void check(value v,field f,void *p) {
	printf("f -> %s\n",val_string(val_field_name(f)));
}

value hxPackageSearch() {
	ScintillaObject *sci = doc_list[hxLocalData.idx].sci;
	struct TextToFind *ttf = g_new(struct TextToFind,1) ;
	ttf->lpstrText = g_strdup("^package .*;");
	ttf->chrg.cpMin = 0 ;
	ttf->chrg.cpMax = geany->sci->get_length(sci);
	gint pos = SSM(sci, SCI_FINDTEXT, SCFIND_REGEXP, (sptr_t)ttf)	;
	value pkg = val_null;
	if (pos > -1) {
		gint bufSize = ttf->chrgText.cpMax - ttf->chrgText.cpMin + 1 ;
		char *buf = g_new0(char,bufSize) ;
		struct TextRange *tr = g_new(struct TextRange,1);
		tr->chrg = ttf->chrgText ;
		tr->lpstrText = buf ;
		SSM(sci, SCI_GETTEXTRANGE,0,(sptr_t) tr);
		pkg = alloc_string(buf) ;
		g_free(buf) ;
		g_free(tr);
	}
	g_free(ttf->lpstrText);
	g_free(ttf) ;
	return pkg ;
}

void hxCheckPlatform(ScintillaObject *sci,value extras,int start) {

	struct TextToFind *ttf = g_new(struct TextToFind,1) ;
	ttf->lpstrText = g_strdup("#end");
	ttf->chrg.cpMin = start ;
	ttf->chrg.cpMax = 0;

	gint style;
	gint endPos = start;
	while(endPos > -1) {
		endPos = SSM(sci, SCI_FINDTEXT, 0 /* SCFIND_WORDSTART */, (sptr_t)ttf);
		style = SSM(sci, SCI_GETSTYLEAT, endPos, 0);

		if (style == SCE_C_PREPROCESSOR) break; // Match
		ttf->chrg.cpMin = endPos;
	}

	g_free(ttf->lpstrText);

	ttf->lpstrText = g_strdup("#");
	ttf->chrg.cpMin = start;
	ttf->chrg.cpMax = endPos;

	gint condPos = start;
	while(condPos > -1) {
		condPos = SSM(sci, SCI_FINDTEXT, 0 /* SCFIND_WORDSTART */, (sptr_t)ttf);
		style = SSM(sci, SCI_GETSTYLEAT, condPos, 0);

		if (style == SCE_C_PREPROCESSOR) break; // Match
		ttf->chrg.cpMin = condPos;
	}

	g_free(ttf->lpstrText);

	if (condPos > -1 && condPos > endPos) {
		int line = geany->sci->get_line_from_position(sci,condPos) ;
		value match = alloc_string(geany->sci->get_line(sci,line)) ;
		val_ocall1(extras,val_id("condPlatform"),match);
	}
	else if(endPos > -1)
		val_ocall0(extras,val_id("resetPlatform"));

	g_free(ttf);
}

/*
 * API FUNCTIONS
 */

value hxSetHaxeLibraryPath(value path)	{
	val_check(path,string);
	char * str = val_string(path);
	hxLocalData.HAXE_LIBRARY_PATH = g_strdup(str) ;
	printf("haxe path is:%s !!\n",hxLocalData.HAXE_LIBRARY_PATH);
	return val_true;
}

value hxPluginDir() {
	return alloc_string(hxLocalData.plugin_dir);
}

value hxProjectDesc() {
	GeanyProject *gp = geany->app->project;
	if (gp != NULL && gp->description != NULL)
		return alloc_string((char *)gp->description);
	return alloc_string("");
}

value hxProjectBase() {

	if(hxLocalData.projectBase != NULL)
		g_free(hxLocalData.projectBase);

	GeanyProject *gp = geany->app->project;
	if (gp != NULL && gp->base_path != NULL) {
		hxLocalData.projectBase = g_strdup(gp->base_path) ;
		return alloc_string((char *)gp->base_path);
	}
	return val_null;
}

value hxStatusAdd(value msg) {
	val_check(msg,string);
	geany->msgwindow->status_add(val_string(msg));
	return val_true;
}

value hxCompilerAdd(value msg) {
	val_check(msg,string);
	// 0 is RED
	((BuildInfo *)(geany->build_info))->dir = hxLocalData.projectBase ;
	geany->msgwindow->compiler_add(0,val_string(msg));
	return val_true;
}

value hxDocumentCreate(value name,value type) {
	val_check(name,string);
//	val_check(type,string);
	geany->document->new_file(val_string(name),NULL,NULL);
	return hxDocumentCurrent();
}

value hxDocumentOpen(value name) {
	geany->document->open_file(val_string(name),FALSE,NULL,NULL) ;
	return hxDocumentCurrent();
}

value hxDocumentCurrent() {
	struct document* d = geany->document->get_current();
	return alloc_abstract(k_hxDoc,d);
}

value hxDocumentIndex() {
	return alloc_int(geany->document->get_cur_idx());
}

value hxDocumentRemove(value ind) {
	geany->document->remove(val_int(ind));
	return val_true;
}

// functions using a document reference from neko ...

value hxDocumentSetText(value gDoc,value text) {
	val_check_kind(gDoc,k_hxDoc);
	val_check(text,string);
	struct document* doc = val_data(gDoc);
	geany->sci->set_text(doc->sci,val_string(text)) ;
	return val_true;
}

value hxDocumentInsertText(value gDoc,value pos,value text) {
	val_check_kind(gDoc,k_hxDoc);
	val_check(pos,int);
	val_check(text,string);
	struct document* doc = val_data(gDoc);
	geany->sci->insert_text(doc->sci,val_int(pos),val_string(text));
	return val_true;
}

value hxDocumentPosition(value gDoc){
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	return alloc_int(geany->sci->get_current_position(doc->sci));
}

value hxDocumentSetPosition(value gDoc,value pos) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	geany->sci->set_current_position(doc->sci,val_int(pos),FALSE);
	return val_true;
}

value hxDocumentGetText(value gDoc) {
	val_check_kind(gDoc,k_hxDoc);
	gchar *data;
	struct document* doc = val_data(gDoc);
	int len = geany->sci->get_length(doc->sci)+1;
	data = (gchar*) g_malloc(len);
	geany->sci->get_text(doc->sci, len, data);
	value d = alloc_string(data);
	g_free(data);
	return d;
}

value hxDocumentGetLength(value gDoc) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	return alloc_int(geany->sci->get_length(doc->sci));
}

value hxDocumentGetSelectedText(value gDoc) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	char *selection = g_malloc(geany->sci->get_selected_text_length(doc->sci)+1);
	geany->sci->get_selected_text(doc->sci, selection);
	return alloc_string(selection);
}

value hxDocumentGetSelectedLength(value gDoc) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	return alloc_int(geany->sci->get_selected_text_length(doc->sci));
}

value hxDocumentSearch(value gDoc,value str) {
	struct document* doc = val_data(gDoc);
	struct TextToFind *ttf = g_new(struct TextToFind,1) ;
	ttf->lpstrText = val_string(str) ;
	ttf->chrg.cpMin = 0 ;
	ttf->chrg.cpMax = geany->sci->get_length(doc->sci);
	gint pos = SSM(doc->sci, SCI_FINDTEXT, SCFIND_REGEXP,(sptr_t) ttf)	;
	g_free(ttf) ;
	return alloc_int(pos) ;
}

value hxDocumentGetLineFromPosition(value gDoc,value pos) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	return alloc_int(geany->sci->get_line_from_position(doc->sci,val_int(pos)));
}

value hxDocumentGetLine(value gDoc,value line) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	return alloc_string(geany->sci->get_line(doc->sci,val_int(line)));
}

value hxDocumentGetCharAt(value gDoc,value pos) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	return alloc_int(geany->sci->get_char_at(doc->sci,val_int(pos)));
}

value hxDocumentGetColFromPosition(value gDoc,value pos) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	return alloc_int(geany->sci->get_col_from_position(doc->sci,val_int(pos)));
}

value hxDocumentReplaceSelection(value gDoc,value text) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	geany->sci->replace_sel(doc->sci,val_string(text));
	return val_true;
}

value hxDocumentGetSelectionStart(value gDoc) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	return alloc_int(geany->sci->get_selection_start(doc->sci));
}

value hxDocumentGetSelectionEnd(value gDoc) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	return alloc_int(geany->sci->get_selection_end(doc->sci));
}

value hxDocumentSetSelectionStart(value gDoc,value pos) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	geany->sci->set_selection_start(doc->sci,val_int(pos));
	return val_true;
}

value hxDocumentSetSelectionEnd(value gDoc,value pos) {
	val_check_kind(gDoc,k_hxDoc);
	struct document* doc = val_data(gDoc);
	geany->sci->set_selection_end(doc->sci,val_int(pos));
	return val_true;
}
