module outermost {
  module middlemost {
    module innermost {
      proc f() {
        writeln("hello from innermost");
      }
    }
  }
}

proc main() {
  use notoutermost.middlemost.innermost;
}

