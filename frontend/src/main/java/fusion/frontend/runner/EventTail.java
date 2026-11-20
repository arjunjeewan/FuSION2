package fusion.frontend.runner;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.channels.SeekableByteChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.NoSuchFileException;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.ArrayList;
import java.util.List;

/**
 * Follows a file that another process appends to, such as {@code events.jsonl} or the run log
 * (A2, 09.C10, 09.C11): each poll reads the new bytes from the last offset, keeps an incomplete
 * last line for the next poll, and returns the complete lines, tolerating a trailing {@code \r}. A
 * file that appears late is fine; one that shrinks is an error. Reads go through Java NIO, which
 * opens files with delete sharing on Windows (section 10, Platform notes).
 */
public final class EventTail {
    /** Bytes read per poll at most, so one huge log cannot stall a tick. */
    static final int MAX_READ = 1 << 20;

    private final Path file;
    private long offset;
    private final ByteArrayOutputStream partial = new ByteArrayOutputStream();

    public EventTail(Path file) {
        this.file = file;
    }

    /** One complete line and the byte offset where it starts. */
    public record Line(long offset, String text) {}

    public Path file() {
        return file;
    }

    /** The bytes consumed so far, an incomplete last line included. */
    public long offset() {
        return offset;
    }

    public List<Line> poll() throws IOException {
        long size;
        try {
            size = Files.size(file);
        } catch (NoSuchFileException e) {
            return List.of();
        }
        if (size < offset) {
            throw new IOException(file + " shrank from " + offset + " to " + size + " bytes");
        }
        if (size == offset) {
            return List.of();
        }
        int want = (int) Math.min(MAX_READ, size - offset);
        ByteBuffer buffer = ByteBuffer.allocate(want);
        try (SeekableByteChannel ch = Files.newByteChannel(file, StandardOpenOption.READ)) {
            ch.position(offset);
            while (buffer.hasRemaining() && ch.read(buffer) > 0) {
                // read until the buffer is full or the file ends
            }
        }
        buffer.flip();
        long lineStart = offset - partial.size();
        List<Line> lines = new ArrayList<>();
        while (buffer.hasRemaining()) {
            byte b = buffer.get();
            offset++;
            if (b == '\n') {
                byte[] bytes = partial.toByteArray();
                int len = bytes.length;
                if (len > 0 && bytes[len - 1] == '\r') {
                    len--;
                }
                lines.add(new Line(lineStart, new String(bytes, 0, len, StandardCharsets.UTF_8)));
                partial.reset();
                lineStart = offset;
            } else {
                partial.write(b);
            }
        }
        return lines;
    }
}
