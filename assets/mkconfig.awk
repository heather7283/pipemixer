BEGIN {
    print "#include <stddef.h>"
    print "const char default_config[] ="
}

/^[^;#]/ {
    gsub(/["\\]/, "\\\\&")
    print "\t\"" $0 "\\n\""
}

END {
    print ";"
    print "const size_t default_config_len = sizeof(default_config) - 1;"
}

