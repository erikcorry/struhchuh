objects = search.o search2.o

search: $(objects)
	clang++ -O3 -o search $(objects)

$(objects): %.o: %.cc
	clang++ -c -O3 $< -o $@

clean:
	rm $(objects) search
