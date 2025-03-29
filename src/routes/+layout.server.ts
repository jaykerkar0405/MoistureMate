interface Event {
	locals: Locals;
}

interface Session {
	[key: string]: any;
}

interface Locals {
	auth: () => Promise<Session>;
}

export const load = async (event: Event) => {
	const session: Session = await event.locals.auth();

	return {
		session
	};
};
