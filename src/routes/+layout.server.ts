// import { redirect } from '@sveltejs/kit';

// interface Event {
// 	locals: Locals;
// 	url: URL;
// }

// interface Session {
// 	[key: string]: any;
// }

// interface Locals {
// 	auth: () => Promise<Session | null>;
// }

// export const load = async (event: Event) => {
// 	const publicRoutes = ['/auth'];
// 	const pathname = event.url.pathname;
// 	const session = await event.locals.auth();

// 	if (!session && !publicRoutes.some((route) => pathname.startsWith(route))) {
// 		throw redirect(302, '/auth');
// 	}

// 	if (session && pathname === '/auth') {
// 		throw redirect(302, '/');
// 	}

// 	return { session };
// };
