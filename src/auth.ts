import {
	AUTH_GITHUB_ID,
	AUTH_GOOGLE_ID,
	AUTH_GITHUB_SECRET,
	AUTH_GOOGLE_SECRET
} from '$env/static/private';
import { PrismaClient } from '@prisma/client';
import { SvelteKitAuth } from '@auth/sveltekit';
import Google from '@auth/sveltekit/providers/google';
import GitHub from '@auth/sveltekit/providers/github';

const prisma = new PrismaClient();

export const { handle, signIn } = SvelteKitAuth({
	providers: [
		Google({ clientId: AUTH_GOOGLE_ID, clientSecret: AUTH_GOOGLE_SECRET }),
		GitHub({ clientId: AUTH_GITHUB_ID, clientSecret: AUTH_GITHUB_SECRET })
	],
	callbacks: {
		async signIn({ user, account }) {
			try {
				const existingUser = await prisma.user.findUnique({
					where: { email: user.email ?? undefined }
				});

				if (!existingUser) {
					await prisma.user.create({
						data: {
							name: user.name,
							image: user.image,
							email: user.email ?? '',
							provider: account?.provider || '',
							providerAccountId: account?.providerAccountId ?? ''
						}
					});
				}

				return true;
			} catch (error) {
				console.error('Error during user creation:', error);
				return false;
			}
		}
	}
});
