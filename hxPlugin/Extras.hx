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
 *
 * 		Russel Weir (a.k.a Madrok)
 */

import neko.io.Path;
import neko.io.Process;

import Geany;

/** Project basePath, classPaths, compile options, and haXe libs. **/
typedef ProjectInfo = {
	var platform	: String;
	var swfVer		: Int;
	var base		: String;
	var cp			: Array<String>;
	var options		: Array<String>;
	var libs		: Array<String>; // Semi future-proof for when we get a nice haXelib gui ;-)
}

/** Thought this might be a little neater **/
private class Reg implements haxe.Public
{
	/** build.hxml option **/
	static var opt			= ~/^\s*(.+?)\s+(.+)$/i;
	/** #if platform conditional compilation **/
	static var condComp		= ~/#(if|else)\s+(flash[5-9]?|neko|js)\s+.*$/;
	static var type			= ~/<type>(.*?)<\/type>/s;
	static var list			= ~/<list>(.*?)<\/list>/s;
	static var syntaxError	= ~/^(.*?):(\d+?):(.*?):(.*?)$/m;
	static var platform		= ~/.*?access the ([A-Za-z]+) package/s;
	static var pkg			= ~/package\s+(.*);/;
	static var dontBugMe	= ~/.*?No classes found in trace.*?/;
	/** Matches with text that end in a keyword like "adasdasd if (" **/
	static var keywordEnd	= ~/^(\s+|.*?\s*)(if|switch|while|for)\s*[(.]\s*$/;
}

/*
 * Class is the entry point for any extras supplied by the plugin
 * written in haXe.
 * Right now, it's just autocompletion, and runModule().
 */
class Extras {
	// Statics
	public static var extras = new Extras();
	public static var VER	 = "0.11";
	public static var TMP	 : String;

	public static function main() {
		extras = new Extras();

		TMP = switch(neko.Sys.systemName())
		{
			case "Windows":		 neko.Sys.getEnv('TEMP')+'\\hxDev';
			case "GNU/kFreeBSD": '/tmp/hxDev';
			case "Linux":		 '/tmp/hxDev';
			case "BSD":			 '/tmp/hxDev';
			case "Mac":			 '/tmp/hxDev';
			//	Someone tell me how to get the tempoary path on Mac!
		}

		// Make hxDev temp directory
		try neko.FileSystem.createDirectory(TMP) catch(e:Dynamic)
			try	neko.FileSystem.isDirectory(TMP) catch(e:Dynamic)
				Geany.statusMsg("hxDev ERROR: Cannot write to "+TMP);

		neko.Lib.println("** NEKO: Plugin 'Extras' "+VER+" initialised.");
	}

	// Instance
	var project : ProjectInfo;
	var curClass(default, setClass) : String;
	var curPlatform(default, setPlatform) : String;
	var curSwfVer(default, setSwfVer) : Int;

	// Cache vars
	/** 'Manually' updated cache of space-character escaped classpath switches. **/
	var cachedClassPath : Array<String>;
	/** Array ready to have file details pushed.
		Reason: I figured completion happens alot more (about every line)
		then actually switching between classes or platforms. **/
	var cachedParams : Array<String>;

	/** Amount of tries the checkPlatform() function may switch platforms. **/
	var checkLimit : Int;

	/** Full path to the current .hx file **/
	var fullPath : String;
	/** Full path to the current tmp hx file, used for autocompletion and syntax checks. **/
	var tmpFile : String;

	// Setters (which handle updating some of the caches)
	function setClass(s:String):String
	{
		curClass = s;
		updateCache();
		return s;
	}

	function setPlatform(s:String):String
	{
		curPlatform = s;
		updateCache();
		return s;
	}

	function setSwfVer(i:Int):Int
	{
		curSwfVer = i;
		if(curPlatform == 'swf') updateCache();
		return i;
		
	}

	// Construct
	public function new()
	{
		checkLimit = 1;

		this.project = {
			platform : 'js', // Default to JS
			swfVer	 : 0,
			base	 : null,
			cp		 : [],
			options  : ['--no-output'],
			libs	 : []
		};
	}

	/** Updates the cache for space-character escaped classpath compiler options **/
	function updateClassPathCache()
	{
		cachedClassPath = ['-cp', TMP+project.base];

		var a = cachedClassPath,
			p = this.project;

		for(path in p.cp) {
			a.push('-cp');
			a.push(path);
		}
		for(l in p.libs) {
			a.push('-lib');
			a.push(l);
		}
	}

	/** Updates the cache for the majority of compiler options **/
	function updateCache()
	{
		if(cachedClassPath == null) return;
		if(curClass == null) {
			cachedParams = ['--display'];
			return;
		}

		var a = this.project.options.concat(cachedClassPath);
		a.push('-'+curPlatform); a.push('no');	// No file, cause no ouput :)
		a.push(curClass);

		if(curPlatform == 'swf' && curSwfVer > 0) {
			a.push('-swf-version');
			a.push(Std.string(curSwfVer));
		}
		a.push("--display");

		cachedParams = a;
	}

	function resetCaches(cp:String)
	{
		var p = project;
		// Fixes bug with open files outside current project
		p.base = cp;
		p.options = [];
		p.cp = [cp];
		p.libs = [];
	}

	/** Returns true when options we're successfully set,
		false when build.hxml could not be found. **/
	function setCompilerOpts(fp:String)
	{
		var pb:String;
		try {
			pb = Geany.getProjectBase();
			if( pb != null ) {
				if ( pb == project.base && pb == fp.substr(0, pb.length) )
					return true;
				else // Reset project base when we're editing a file outside of project
					project.base = null;
			}
		} catch(e:Dynamic) trace(Std.string(e));


		/* try setting classpath based on package name or lack thereof */
		var pkg = Geany.getPackage(),
			cp:String,
			ac:String,
			o = this.project,
			h = new Hash<Bool>();

		if (pkg == null) // classpath is the path to the file, add it ...
			cp = neko.io.Path.directory(fp);

		else try {
			if (Reg.pkg.match(pkg)) {
				var p = Reg.pkg.matched(1);
				var extra = (StringTools.endsWith(fp,"/")) ? 1 : 0 ;
				cp = neko.io.Path.directory(fp).substr(0,-(p.length+extra));
				//BD ??
				//var i = cp.lastIndexOf('.');
				//curClass = if(i != -1) cp.substr(i); else cp;
				//trace("Curclass is now "+curClass);
			} else trace("shouldn't get here");
		} catch(e:Dynamic) trace(e);

		// Set classpath in doublecheck Hashtable
		h.set(cp, true);

		/* try setting class path from build.hxml in the
		 * projects base dir */
		try if (pb == null){ // Currently no project opened
			setStatus("Couldn't find: "+ ac +", class paths will be guessed instead.");
			resetCaches(cp);
		} else {
			ac = pb + "/build.hxml";
			trace('Checking ['+cp+'] for build.hxml: '+ac);
			if(!neko.FileSystem.exists(ac)) {
				setStatus("Couldn't find: "+ ac +", class paths will be guessed instead.");
				resetCaches(cp);
				updateClassPathCache();
				return false;
			} else {
				o.cp.push(cp);
			}

			var s = neko.io.File.getContent(ac);

			// Saves neko some property lookups :-)
			var sw = StringTools.startsWith,
				rOpt = Reg.opt,
				line:String; // Prevent duplicates

			resetCaches(cp);

			// Add usefull compiler switches

			for( fileLine in s.split("\n") )
			{
				line = StringTools.trim(fileLine);
				// Skip empty and commented lines
				if( line.length == 0 || line.substr(0,1) == '#' ) continue;
				// Only parse first target
				//if( line == '--next' ) break;

				// Check if this line has any parameter set at all
				if(rOpt.match(line)) switch(rOpt.matched(1))
				{
					case "-lib":
						if(!h.exists(line=rOpt.matched(2)) ) {
							o.libs.push(line);
						//	trace("pushed lib:"+line);
							h.set(line, true);
						}
					case "-cp":
						if(!h.exists(line=rOpt.matched(2)) ) { // Prevent double classpaths
							o.cp.push(line);
						//	trace("pushed cp:"+line);
							h.set(line, true);
						}
					case "-cmd":
						continue;
					case "-neko":
						if (o.platform == null)
						o.platform = curPlatform = 'neko';
					case "-js":
						if (o.platform == null)
						o.platform = curPlatform = 'js';
					case "-swf":
						if (o.platform == null)
						o.platform = curPlatform = 'swf';
					case "-swf-version":
						if (o.swfVer == null)
						o.swfVer = curSwfVer = Std.parseInt(rOpt.matched(1));
				}
				else {
					if( line != '--next' ){
						o.options.push(line);
						trace("Adding unlisted: "+line);
					}
				}
			}
		} catch (exc:Dynamic) {
			setStatus("No read permissions for: "+ ac);
		}

		// Add classpath if it wasnt allready set
		if(!h.exists(cp)) o.cp.push(cp);

		// Update cache
		updateClassPathCache();
		return true;
	}

	/**
	 * changeDocument is called from nekoplg.c on each document activate
	 * event.
	 **/
	public function changeDocument(ifn:String,idx:Int)
	{
		fullPath = new String(ifn); //convert from neko string

		if(tmpFile != null) try	{
			neko.FileSystem.deleteFile(tmpFile);
		} catch(e:Dynamic){};

		try {
			tmpFile = TMP+fullPath;
			var p = new neko.io.Path(fullPath),
				s = (p.backslash)?'\\':'/',
				x:String = TMP,
				c = neko.FileSystem.createDirectory;

			if(p.dir != null) for( d in p.dir.split(s) ) {
				try c(x+=(s+d)) catch(e:Dynamic){} // Directory probably allready exists :)
			}

		} catch(e:Dynamic) trace("Didn't work :-( "+e);

		trace(fullPath);
		
		setCompilerOpts(fullPath);
		if(curPlatform == null) curPlatform = project.platform;

		var filetocompile:String;
		if(project.cp != null)
		  for (cp in project.cp) {
			if (cp == fullPath.substr(0,cp.length)) {
				var pack = fullPath.substr(cp.length+1);
				filetocompile = StringTools.replace(pack,"/",".");
				var s = (filetocompile.charAt(0) == ".") ? 1 : 0 ;
				filetocompile = filetocompile.substr(s,filetocompile.length-3);
			}
		}
		if (filetocompile != null && filetocompile.length > 0) {
			curClass = filetocompile;
			return true;
		}
		curClass = null;
		return false;
	}

	function runHaxe(pos:Int) : String
	{
		var d = Geany.current(),
			o:String,
			l = d.getLength()+1,
			prms = cachedParams.concat([tmpFile+'@'+pos]);

		try {
			neko.FileSystem.rename(fullPath, fullPath+'~');
			var f = neko.io.File.write(tmpFile, true);
			var i = new neko.io.StringInput(d.getText());
			f.writeInput(i,l);
			f.close();

			trace('Running haXe with: '+prms);
			var h  = new neko.io.Process("haxe",prms);
			if (h.stderr != null) {
				o = h.stderr.readAll();
			}
			trace(h.stdout.readAll());

		} catch(exc:Dynamic) {
			Geany.statusMsg('Calling haXe compiler failed: '+exc);
			trace(exc);
		}
		try neko.FileSystem.rename(fullPath+'~', fullPath)
			catch(e:Dynamic) Geany.statusMsg('Could not rename current file to orignal name. Check for: "'+fullPath+'~" or save the current file.');
		
		return o;
	}

	/**
	 * completeDot, completeBracket and checkLine should only be called
	 * if changeDocument returned true, i.e. the currently selected file
	 * is known to be sitting in the classpath somewhere, so it can be
	 * completed.
	 */
	public function completeDot(pos:Int,line:Int):String
	{
		trace("completeDot("+pos+", "+line+")");
		
		var output = runHaxe(pos),
			rl = Reg.list;

		if (output == null || output.length == 0)
			return null;

		if(output.indexOf("<type>") != -1){
			var e = "SYN:Expected a (";
			return  untyped e.__s;
		}

		if (!rl.match(output)) {
			// If we can change platforms successfully, go ahead
			if(	checkLimit <= 0 )
				checkLimit = 1;
			else if( checkPlatform(output) )
				return completeDot(pos,line);

			if(Reg.syntaxError.match(output))
				output = Reg.syntaxError.matched(4);
			return untyped ("SYN:"+StringTools.trim(output)).__s;
		}

		var xml  = Xml.parse(rl.matched(0)),
			list = new haxe.xml.Fast(xml.firstElement()),
			str  = new StringBuf(),
			skip = true;

		for ( i in list.nodes.i) {
			if(!skip) str.addChar(32); // Equal to: .add(" ");  but faster :-)
			else skip = false;
			str.add(i.att.n);
		}

		return untyped str.toString().__s;
	}

	public function completeBracket(pos:Int,line:Int):String
	{
		trace("completeBracket("+pos+", "+line+")");
		var str = Geany.current().getLine(line);
		if(Reg.keywordEnd.match(str)) return null;
		
		var output = runHaxe(pos),
			rt = Reg.type;
		
		if (output == null) return null;

		if(output.indexOf("<list>") != -1){
			var e = "SYN:Expected a . ";
			return  untyped  e.__s;
		}

		if (!rt.match(output)) {
			// If we can change platforms successfully, go ahead
			if(	checkLimit <= 0 )
				checkLimit = 1;
			else if( checkPlatform(output) )
				return completeBracket(pos,line);

			if(Reg.dontBugMe.match(output))
				return null; // Yes haXe, I know there are no classes in trace ..!
			if(Reg.syntaxError.match(output))
				output = Reg.syntaxError.matched(4);
			return untyped ("SYN:"+output).__s;
		}
		

		var s:String;
		try {
			s = StringTools.trim( StringTools.htmlUnescape(
				Xml.parse(rt.matched(0)).firstChild().firstChild().nodeValue
			) );
		} catch(e:Dynamic) return untyped Std.string(e).__s;

		return untyped s.__s;
	}

	public function checkLine(pos:Int,line:Int):String
	{
		var output = runHaxe(pos),
			rse = Reg.syntaxError;

		if (output == null)
			return null;

		var errors = output.split("\n");

		for (e in errors) {
			if (rse.match(e)) {
				var f = neko.io.Path.withoutDirectory(rse.matched(1));
				if(f == this.tmpFile) f = this.fullPath; // Tmp file error underlining fix
				Geany.compilerMsg(f+":"+rse.matched(2)+":error:"+rse.matched(4));
				return "SYN:"+output;
			}
		}
		return null;
	}

	/**
	 * Called when the compiler detects a platform switch, and returns
	 * an error. This is the primary method of platform switching, when
	 * no conditional compilation #if etc are in the source file
	 **/
	public function checkPlatform(o:String):Bool
	{
		// Prevent infinite compile loop
		if(checkLimit-- <= 0) return false;

		var rp = Reg.platform;
		if (rp.match(o))
		{
			var newPlatform = StringTools.trim(rp.matched(1));
			if (newPlatform == curPlatform)
				return false;
			changePlatform(newPlatform);
			return true;
		}
		return false;
	}

	/**
	 * Called from C, when a #if or #else has been found, above the
	 * current insertion position, then this function can change the
	 * platform accordingly before compilation occurs.
	 **/
	public function condPlatform(o:String)
	{
		var cond = new String(o);

		if(Reg.condComp.match(cond))
			try changePlatform(Reg.condComp.matched(2))
			catch(e:Dynamic) trace("Impossible error: "+e);
	}

	/**
	 * Core change platform called by checkPlatform and condPlatform
	 **/
	function changePlatform(newPlatform:String)
	{
		if(newPlatform == curPlatform) return; // No platform change

		if (StringTools.startsWith(newPlatform,"flash"))
		{
			var flashVersion = Std.parseInt(newPlatform.substr(5));
			trace('flashVersion: '+flashVersion);
			if(curPlatform == "swf" && curSwfVer == flashVersion) return;
			curPlatform = "swf";
			curSwfVer = flashVersion;
		}
		else curPlatform = newPlatform;
		trace("Changed platform to: "+newPlatform);
	}

	/**
	 * Core reset platform to project global setting.
	 * Called by C when outside of #if conditional blocks.
	 **/
	function resetPlatform()
	{
		trace("Reset platform");
		var p = project;
		if( curPlatform != p.platform )	curPlatform = p.platform;
		if( curSwfVer != p.swfVer )		curSwfVer = p.swfVer;
	}

	function setStatus(msg:String)
	{
		Geany.statusMsg("Autocomplete: "+msg);
	}

	public function runModule(mp)
	{
		trace("Extras.runModule()");
		if (mp != null) {
			try {
				var p = new neko.io.Path(new String(mp));
				Geany.runModule(p.dir,p.file);
			} catch (exc:Dynamic) {
				setStatus("module run exception:"+exc) ;
			}
		} else
			setStatus("Can't run null module");
	}
}
