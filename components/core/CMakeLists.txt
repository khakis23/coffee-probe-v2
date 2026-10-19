# core: pure, hardware-independent C++ (decoders, filters, state machines, policies).
#
# RULE: no IDF dependencies. Do not add REQUIRES/PRIV_REQUIRES on IDF components, and never
# #include esp_*, freertos/*, driver/* or other IDF headers from anything in this component.
# Plain standard C++ only. The same sources are compiled natively on the host by the doctest
# project in test/host (#7), and that build fails the moment an IDF header sneaks in.
# (Inside the firmware build IDF still adds its "common requirements" implicitly, so the
# host build is what actually enforces this rule.)
idf_component_register(
    SRCS "core.cpp"
    INCLUDE_DIRS "include"
)
