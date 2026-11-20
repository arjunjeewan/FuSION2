package fusion.frontend.ui;

import java.time.Clock;
import java.util.UUID;
import java.util.function.Supplier;

import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.context.annotation.Bean;

/**
 * The Spring Boot application (G2, 11.C21), with no web server (10.C1): it scans the five modules of
 * 10.C11 and gives them their time and id sources, which tests replace (section 10, Determinism).
 */
@SpringBootApplication(scanBasePackages = "fusion.frontend")
public class FrontendConfiguration {
    /** Local time names run directories (09.C1); instants stay UTC. */
    @Bean
    public Clock clock() {
        return Clock.systemDefaultZone();
    }

    @Bean
    public Supplier<UUID> ids() {
        return UUID::randomUUID;
    }
}
