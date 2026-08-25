// The README's `### Code generator` C++ example, as code that compiles and runs
// — and a check that the README still shows calls this code implements.
//
// A hand-written snippet in a Markdown file stops matching the API the moment
// the API moves, and nothing notices until a reader copies it. Point below is a
// hand-written stand-in for what `sofabgen --lang cpp` emits, built only out of
// the wrapper this repo ships, and the mirror check fails if the README's
// snippet reaches for something it does not have. Nothing asserts what the
// README *says* — only that the code in it is code that exists, and that it
// works.

#include "sofab/sofab.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

// What `sofabgen --lang cpp` emits, reduced to the surface §6.1.1 closes.
struct Point : sofab::Message
{
    static constexpr std::size_t _maxSize = 32;
    int32_t x = 0, y = 0;

    sofab::OStreamImpl::Result serialize(sofab::OStreamImpl &os) const noexcept override
    {
        return os.write(1, x).write(2, y);
    }

    void deserialize(sofab::IStreamImpl &is, sofab::id id, size_t, size_t) noexcept override
    {
        switch (id)
        {
        case 1: is.read(x); break;
        case 2: is.read(y); break;
        }
    }

    std::vector<uint8_t> encode() const
    {
        sofab::OStreamInline<_maxSize> os;
        serialize(os);
        os.flush();
        return {os.data(), os.data() + os.bytesUsed()};
    }

    static Point decode(const uint8_t *data, size_t len)
    {
        sofab::IStreamObject<Point> in;
        in.feed(data, len);
        return *in;
    }
};

static int failures = 0;

static void check(bool ok, const char *what)
{
    if (!ok)
    {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

// The snippet's three lines, run: encode, decode, and the values survive.
static void theExampleRunsAndRoundTrips()
{
    Point pt; pt.x = 3; pt.y = 4;
    std::vector<uint8_t> wire = pt.encode();
    Point got = Point::decode(wire.data(), wire.size());
    check(got.x == 3 && got.y == 4, "one-shot round trip");

    // The streaming pair must produce the one-shot bytes through a window far
    // smaller than the message — MIN_OUTPUT_BUFFER is 1 on this port.
    std::vector<uint8_t> streamed;
    sofab::OStreamInline<1> os([&streamed](std::span<const uint8_t> chunk) {
        streamed.insert(streamed.end(), chunk.begin(), chunk.end());
    });
    pt.serialize(os);
    os.flush();
    check(streamed == wire, "a one-byte window produces the one-shot bytes");

    // Fed one byte at a time, the same bytes decode to the same value.
    sofab::IStreamObject<Point> in;
    auto r = in.feed(wire.data(), 1);
    for (size_t i = 1; i < wire.size(); ++i) r = in.feed(wire.data() + i, 1);
    check(r.ok(), "byte-at-a-time feed reports a complete message");
    check((*in).x == 3 && (*in).y == 4, "byte-at-a-time decode");
}

// Every call the README's snippet makes must exist on the stand-in above.
static void theReadmeSnippetCallsOnlyWhatThisFileImplements()
{
    std::ifstream f(SOFAB_README_PATH);
    if (!f)
    {
        std::fprintf(stderr, "FAIL: cannot open %s\n", SOFAB_README_PATH);
        ++failures;
        return;
    }
    std::stringstream ss; ss << f.rdbuf();
    const std::string doc = ss.str();

    const std::string heading = "### Code generator";
    size_t i = doc.find(heading);
    if (i == std::string::npos)
    {
        std::fprintf(stderr, "FAIL: no `%s` section\n", heading.c_str());
        ++failures;
        return;
    }
    std::string rest = doc.substr(i);
    if (size_t e = rest.find("\n## ", heading.size()); e != std::string::npos)
        rest = rest.substr(0, e);
    size_t s = rest.find("```cpp");
    if (s == std::string::npos)
    {
        std::fprintf(stderr, "FAIL: the section has no ```cpp example\n");
        ++failures;
        return;
    }
    rest = rest.substr(s + 6);
    size_t e = rest.find("```");
    const std::string block = rest.substr(0, e);

    std::ifstream self(SOFAB_SELF_PATH);
    std::stringstream ms; ms << self.rdbuf();
    const std::string mirror = ms.str();

    for (const char *call : {"encode()", "decode(", ".x = 3", ".y = 4"})
    {
        if (block.find(call) == std::string::npos)
        {
            std::fprintf(stderr, "FAIL: the README snippet no longer shows `%s`\n", call);
            ++failures;
        }
        if (mirror.find(call) == std::string::npos)
        {
            std::fprintf(stderr,
                         "FAIL: this file does not implement `%s`, which the README calls\n",
                         call);
            ++failures;
        }
    }
}

int main()
{
    theExampleRunsAndRoundTrips();
    theReadmeSnippetCallsOnlyWhatThisFileImplements();
    if (failures == 0) std::puts("test_readme_example: all checks passed");
    return failures == 0 ? 0 : 1;
}
