CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
INCDIR = ../include
BUILDDIR = build

test_scanner: $(BUILDDIR)/test_scanner
	./$(BUILDDIR)/test_scanner

test_import: $(BUILDDIR)/test_import
	./$(BUILDDIR)/test_import

test_dot: $(BUILDDIR)/test_dot
	./$(BUILDDIR)/test_dot

debug_tokens: $(BUILDDIR)/debug_tokens
	./$(BUILDDIR)/debug_tokens

$(BUILDDIR)/test_scanner: test_scanner.cpp $(INCDIR)/scanner.h $(INCDIR)/token.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) ../src/scanner.cpp ../src/token.cpp test_scanner.cpp -o $(BUILDDIR)/test_scanner

$(BUILDDIR)/test_import: test_import.cpp $(INCDIR)/scanner.h $(INCDIR)/token.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) ../src/scanner.cpp ../src/token.cpp test_import.cpp -o $(BUILDDIR)/test_import

$(BUILDDIR)/test_dot: test_dot.cpp $(INCDIR)/scanner.h $(INCDIR)/token.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) ../src/scanner.cpp ../src/token.cpp test_dot.cpp -o $(BUILDDIR)/test_dot

$(BUILDDIR)/debug_tokens: debug_tokens.cpp $(INCDIR)/scanner.h $(INCDIR)/token.h
	@mkdir -p $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -I$(INCDIR) ../src/scanner.cpp ../src/token.cpp debug_tokens.cpp -o $(BUILDDIR)/debug_tokens

clean:
	rm -rf $(BUILDDIR)