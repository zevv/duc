
duc.1 duc.html: manual.txt ../duc/duc
	../duc/duc manual > options.txt
	sed -e "/>>>>/r options.txt" -e "//d" < manual.txt > duc.txt
	ronn duc.txt

clean:
	rm -f duc.1 duc.txt options.txt duc.1.html
