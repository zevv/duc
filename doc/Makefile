
duc.1 duc.html: manual.txt ../duc/duc
	../duc/duc manual > options.txt
	sed -e "/>>>>/r options.txt" -e "//d" < manual.txt > duc.md
	ronn duc.md

clean:
	rm -f options.txt 
