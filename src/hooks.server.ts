import { sequence } from '@sveltejs/kit/hooks';
import { redirect, type Handle } from '@sveltejs/kit';
import { handle as authenticationHandle } from './auth';

const authorizationHandle: Handle = async ({ event, resolve }) => {
	const session = await event.locals.auth();

	if (!session && !event.url.pathname.startsWith('/auth')) {
		throw redirect(303, '/auth');
	}

	if (session && event.url.pathname.startsWith('/auth')) {
		throw redirect(303, '/');
	}

	return resolve(event);
};

export const handle: Handle = sequence(authenticationHandle, authorizationHandle);
