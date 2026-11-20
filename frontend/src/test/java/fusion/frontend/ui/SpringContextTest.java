package fusion.frontend.ui;

import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.nio.file.Path;

import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.springframework.boot.Banner;
import org.springframework.boot.WebApplicationType;
import org.springframework.boot.builder.SpringApplicationBuilder;
import org.springframework.context.ConfigurableApplicationContext;

import fusion.frontend.accounts.AccountService;
import fusion.frontend.pipeline.TestFixtures;
import fusion.frontend.runner.DefaultRunService;
import fusion.frontend.sessions.SessionStore;
import fusion.frontend.sessions.SessionUse;

/**
 * The application's Spring context starts with no web server and no port (10.C1, 11.C21), wires the
 * five modules, and shuts down without touching runs (10.C16). No JavaFX is started.
 */
class SpringContextTest {
    @TempDir
    Path tmp;

    @Test
    void theContextWiresEveryServiceWithoutAWebServer() {
        try (ConfigurableApplicationContext c = new SpringApplicationBuilder(FrontendConfiguration.class)
                .web(WebApplicationType.NONE).bannerMode(Banner.Mode.OFF).logStartupInfo(false)
                .properties("fusion.data-dir=" + tmp.resolve("data"), "fusion.results-dir=" + tmp.resolve("runs"),
                        "fusion.backend.executable=" + tmp.resolve("no-fusion"),
                        "fusion.accounts.bcrypt-strength=10", "logging.level.root=warn")
                .run()) {
            Services s = Services.from(c, Runnable::run, p -> { }, null);
            assertTrue(s.accounts().isEmpty());
            assertTrue(c.getBean(SessionUse.class) instanceof DefaultRunService);
            assertTrue(c.getBean(SessionStore.class).list(new fusion.frontend.accounts.AccountId(java.util.UUID.randomUUID()))
                    .isEmpty());
            assertFalse(c.containsBean("tomcatServletWebServerFactory"));
            assertTrue(c.getBean(AccountService.class) != null);
            // The default pipeline comes from FUSION_HOME when fusion cannot say (11.C20); here neither can.
            assertTrue(TestFixtures.standardPipelineFile().toFile().isFile());
        }
    }
}
