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
 */
class Geany {

	public static function current():Document {
		var ab = null;
		try	{
			ab = untyped __dollar__loader.apiObj.hxDocumentCurrent();
			return new Document(ab);
		} catch(exc:Dynamic) {
			trace("Geany.current():"+exc);
		}
		return null;
	}
	
	/**
	 * add message to the status window
	 **/
	public static function statusMsg(msg:String) {
		untyped __dollar__loader.apiObj.hxStatusAdd(msg.__s);
	}

	/**
	 * add message to the compiler window
	 **/
	public static function compilerMsg(msg:String) {
		untyped __dollar__loader.apiObj.hxCompilerAdd(msg.__s);
	}

	public static function create(name:String,type:String):Document	{
		untyped __dollar__loader.apiObj.hxDocumentCreate(name.__s,null);
		return current();
	}

	public static function open(name:String):Document {
		untyped __dollar__loader.apiObj.hxDocumentOpen(name.__s);
		return current();
	}

	public static function index():Int {
		return untyped __dollar__loader.apiObj.hxDocumentIndex();
	}

	public static function remove(idx:Int) {
		return untyped __dollar__loader.apiObj.hxDocumentRemove(idx);
	}

	public static function getPluginDir() {
		return new String(untyped __dollar__loader.apiObj.hxPluginDir());
	}

	public static function getProjectBase() {
		var pb = untyped __dollar__loader.apiObj.hxProjectBase() ;
		if (pb == null) return null;
		return new String(pb);
	}

	public static function getPackage() {
		var p = untyped __dollar__loader.apiObj.hxPackageSearch();
		if (p == null) return null;
		return new String(p);
	}

	public static function runModule(path:String,name:String) {
		var modulePath = (path.substr(-1,-1) != "/") ? [path+"/"] : [path];
		var moduleName = name ;
		var mod:neko.vm.Module ;

		try{
			var loader = neko.vm.Loader.local();
			mod = loader.getCache().get(moduleName);

			if (mod == null) {
				trace("trying to load "+modulePath);
				trace("name:"+moduleName);
				mod = neko.vm.Module.readPath(moduleName+".n",modulePath,loader);
				loader.setCache(moduleName,mod);
				mod.execute();

				var table = mod.exportsTable();
				var classes = table.__classes;
				var c = Reflect.field(classes,moduleName);
				var inst = Type.createInstance(c,[]);
			} else {
				trace("module already loaded\n");
			}
			return 1;
		} catch (exc:Dynamic) {
			trace("exc loading HxDev:"+exc);

		}
		return 0;
	}
}

class Document {

	var gDoc:Dynamic;

	public function new(gDoc:Dynamic) {
		this.gDoc = gDoc ;
	}

	public function setText(text:String) {
		untyped __dollar__loader.apiObj.hxDocumentSetText(gDoc,text.__s);
	}

	public function insertText(pos:Int,text:String) {
		untyped __dollar__loader.apiObj.hxDocumentInsertText(gDoc,pos,text.__s);
	}

	public function getPosition():Int {
		return untyped __dollar__loader.apiObj.hxDocumentPosition(gDoc);
	}

	public function setPosition(pos:Int) {
		return untyped __dollar__loader.apiObj.hxDocumentSetPosition(gDoc,pos);
	}

	public function getLength():Int {
		return untyped __dollar__loader.apiObj.hxDocumentGetLength(gDoc);
	}

	public function getText():String {
		var s = untyped __dollar__loader.apiObj.hxDocumentGetText(gDoc);
		if (s == null) return null ;
		return new String(s) ;
	}

	public function getSelectedLength() {
		return untyped __dollar__loader.apiObj.hxDocumentGetSelectedLength(gDoc);
	}

	public function getSelectedText():String {
		var s = untyped __dollar__loader.apiObj.hxDocumentGetSelectedText(gDoc);
		if (s == null) return null ;
		return new String(s) ;
	}

	public function getSelectionStart() {
		return untyped __dollar__loader.apiObj.hxDocumentGetSelectionStart(gDoc);
	}

	public function getSelectionEnd() {
		return untyped __dollar__loader.apiObj.hxDocumentGetSelectionEnd(gDoc);
	}

	public function setSelectionStart(pos:Int) {
		untyped __dollar__loader.apiObj.hxDocumentSetSelectionStart(gDoc,pos);
	}

	public function setSelectionEnd(pos:Int) {
		untyped __dollar__loader.apiObj.hxDocumentSetSelectionEnd(gDoc,pos);
	}

	public function search(text:String):Int {
		return untyped __dollar__loader.apiObj.hxDocumentSearch(gDoc,text.__s);
	}

	public function getLineFromPosition(pos:Int) {
		return untyped __dollar__loader.apiObj.hxDocumentGetLineFromPosition(gDoc,pos);
	}

	public function getColFromPosition(pos:Int) {
		return untyped __dollar__loader.apiObj.hxDocumentGetColFromPosition(gDoc,pos);
	}

	public function getLine(line_no:Int):String {
		var s = untyped __dollar__loader.apiObj.hxDocumentGetLine(gDoc,line_no);
		if (s == null) return null;
		return new String(s) ;
	}

	public function getCharAt(pos:Int):Int {
		return untyped __dollar__loader.apiObj.hxDocumentGetCharAt(gDoc,pos);
	}

}
