import {
	AUTH_GITHUB_ID,
	AUTH_GOOGLE_ID,
	AUTH_GITHUB_SECRET,
	AUTH_GOOGLE_SECRET
} from '$env/static/private';
import { SvelteKitAuth } from '@auth/sveltekit';
import Google from '@auth/sveltekit/providers/google';
import GitHub from '@auth/sveltekit/providers/github';

export const { handle, signIn } = SvelteKitAuth({
	providers: [
		Google({ clientId: AUTH_GOOGLE_ID, clientSecret: AUTH_GOOGLE_SECRET }),
		GitHub({ clientId: AUTH_GITHUB_ID, clientSecret: AUTH_GITHUB_SECRET })
	]
});
