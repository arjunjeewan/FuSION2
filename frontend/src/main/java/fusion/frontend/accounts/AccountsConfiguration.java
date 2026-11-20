package fusion.frontend.accounts;

import java.nio.file.Path;
import java.time.Clock;
import java.util.UUID;
import java.util.function.Supplier;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

/** The accounts module's services (10.C11), with {@code fusion.accounts.bcrypt-strength} (10 to 14, default 12). */
@Configuration(proxyBeanMethods = false)
public class AccountsConfiguration {
    @Bean
    public AccountService accountService(@Value("${fusion.data-dir}") Path dataDir,
                                         @Value("${fusion.accounts.bcrypt-strength:12}") int strength,
                                         Clock clock, Supplier<UUID> ids) {
        if (strength < 10 || strength > 14) {
            throw new IllegalArgumentException("fusion.accounts.bcrypt-strength takes 10 to 14, not " + strength);
        }
        return new FileAccountService(dataDir, strength, clock, ids);
    }
}
