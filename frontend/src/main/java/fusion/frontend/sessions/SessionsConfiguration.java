package fusion.frontend.sessions;

import java.nio.file.Path;
import java.time.Clock;
import java.util.UUID;
import java.util.function.Supplier;

import org.springframework.beans.factory.ObjectProvider;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import fusion.frontend.accounts.AccountService;
import fusion.frontend.pipeline.DefaultPipeline;

/** The sessions module's services (10.C11); {@link SessionUse} comes from runner, looked up lazily. */
@Configuration(proxyBeanMethods = false)
public class SessionsConfiguration {
    @Bean
    public SessionStore sessionStore(@Value("${fusion.data-dir}") Path dataDir, DefaultPipeline pipelines,
                                     AccountService accounts, ObjectProvider<SessionUse> use, Clock clock,
                                     Supplier<UUID> ids) {
        return new FileSessionStore(dataDir, pipelines, accounts, use::getObject, clock, ids);
    }

    @Bean
    public SessionChecks sessionChecks(DefaultPipeline pipelines, ObjectProvider<SessionUse> use) {
        return new FileSessionChecks(pipelines, use::getObject);
    }
}
