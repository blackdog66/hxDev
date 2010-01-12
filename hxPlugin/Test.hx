// Commit test
class Test {
	public static function main()
	{
		var t = neko.Sys.time;
		
		trace('concat:');
		
		var a = [1,2,3];
		var start = t();
		for(x in 0 ... 1000000)
			a.concat([4,5,6]);
		var end = t();
		trace(end-start);
		
		
		trace('push:');
		
		var a = [1,2,3];
		var start = t();
		for(x in 0 ... 1000000) {
			var b = [4,5,6];
			a.push(b[0]);
			a.push(b[1]);
			a.push(b[2]);
		}
		var end = t();
		trace(end-start);
	}
}
